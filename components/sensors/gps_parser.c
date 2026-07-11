/* Phân tích cú pháp các bản tin NMEA 0183 (GGA, RMC, GSA) để lấy tọa độ, tốc độ, sai số HDOP và số vệ tinh. */

#include "gps_parser.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Quy đổi ký tự Hex dạng char sang giá trị nguyên */
static int gps_hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool gps_checksum_valid(const char *line) {
    if (!line || line[0] != '$') return false;
    const char *star = strchr(line, '*');
    if (!star || star == line + 1) return false;
    if (star[1] == '\0' || star[2] == '\0') return false;

    int hi = gps_hex_nibble(star[1]);
    int lo = gps_hex_nibble(star[2]);
    if (hi < 0 || lo < 0) return false;

    // Phép XOR liên tiếp các byte ký tự nằm giữa '$' và '*' để tính Checksum thực tế
    unsigned char calc = 0;
    for (const char *p = line + 1; p < star; p++) {
        calc ^= (unsigned char)*p;
    }

    return calc == (unsigned char)((hi << 4) | lo);
}

static bool gps_strip_checksum(char *line) {
    char *star = strchr(line, '*');
    if (!star || star == line + 1) return false;
    if (!gps_checksum_valid(line)) return false;
    *star = '\0'; // Cắt bỏ phần checksum ở cuối bằng cách chèn ký tự kết thúc chuỗi NULL
    return true;
}

/* Chuyển đổi định dạng DDMM.MMMM (Độ Phút) sang Độ thập phân (Decimal Degrees)
   Công thức: Decimal_Degrees = Degrees + (Minutes / 60) */
static double gps_dm_to_decimal(const char *dm, const char hemi) {
    if (!dm || dm[0] == '\0') return 0.0;

    const double raw = atof(dm);
    
    // Tách phần độ (Degrees): Lấy phần nguyên của (DDMM.MMMM / 100)
    const int deg = (int)(raw / 100.0);
    
    // Phần phút (Minutes) là phần còn dư sau khi tách độ
    const double minutes = raw - ((double)deg * 100.0);
    
    // Quy đổi phút sang độ thập phân bằng cách chia cho 60
    double out = (double)deg + (minutes / 60.0);
    
    // Nếu thuộc bán cầu Nam (S) hoặc Tây (W), giá trị độ thập phân có dấu âm (-)
    if (hemi == 'S' || hemi == 'W') {
        out = -out;
    }
    return out;
}

static bool gps_is_sentence(const char *line, const char *type) {
    // Bản tin NMEA hợp lệ bắt đầu bằng '$' và tên bản tin nằm ở byte thứ 3,4,5 (ví dụ: $GPRMC thì RMC ở index 3,4,5)
    return line && type && line[0] == '$' && strlen(line) >= 6 &&
           line[3] == type[0] && line[4] == type[1] && line[5] == type[2];
}

static int gps_split(char *line, char **fields, int max_fields) {
    int count = 0;
    char *p = line;
    fields[count++] = p;
    while (*p && count < max_fields) {
        if (*p == ',') {
            *p = '\0';
            fields[count++] = p + 1;
        }
        p++;
    }
    return count;
}

bool gps_parse_line(char *line, gps_data_t *out) {
    if (!line || !out || line[0] != '$') return false;
    memset(out, 0, sizeof(*out));
    if (!gps_strip_checksum(line)) return false;

    // Phân rã chuỗi NMEA bằng dấu phẩy (tối đa 24 trường)
    char *fields[24];
    int num_fields = gps_split(line, fields, 24);
    if (num_fields < 1) return false;

    // 1. Phân tích Bản tin RMC (Recommended Minimum Navigation Information)
    if (gps_is_sentence(fields[0], "RMC")) {
        char status = 'V';
        char lat_s[20] = {0};
        char lat_h = 'N';
        char lon_s[20] = {0};
        char lon_h = 'E';
        char spd_s[16] = {0};

        if (num_fields > 2 && fields[2][0] != '\0') status = fields[2][0];
        if (num_fields > 3) strncpy(lat_s, fields[3], sizeof(lat_s) - 1);
        if (num_fields > 4 && fields[4][0] != '\0') lat_h = fields[4][0];
        if (num_fields > 5) strncpy(lon_s, fields[5], sizeof(lon_s) - 1);
        if (num_fields > 6 && fields[6][0] != '\0') lon_h = fields[6][0];
        if (num_fields > 7) strncpy(spd_s, fields[7], sizeof(spd_s) - 1);

        out->has_rmc = true;
        out->fix_valid = (status == 'A'); // 'A' = Active (Định vị hợp lệ), 'V' = Void (Không hợp lệ)
        
        if (out->fix_valid) {
            out->latitude_deg = gps_dm_to_decimal(lat_s, lat_h);
            out->longitude_deg = gps_dm_to_decimal(lon_s, lon_h);
            
            // Kiểm tra tính thực tế của tọa độ: Vĩ độ nằm trong [-90.0, 90.0] độ, Kinh độ nằm trong [-180.0, 180.0] độ
            if (lat_s[0] == '\0' || lon_s[0] == '\0' ||
                (lat_h != 'N' && lat_h != 'S') ||
                (lon_h != 'E' && lon_h != 'W') ||
                !isfinite(out->latitude_deg) || !isfinite(out->longitude_deg) ||
                fabs(out->latitude_deg) > 90.0 || fabs(out->longitude_deg) > 180.0) {
                out->fix_valid = false;
                out->latitude_deg = 0.0;
                out->longitude_deg = 0.0;
                return true;
            }
            
            // Quy đổi tốc độ từ Knots (hải lý/giờ) sang km/h (hệ số 1.852f)
            out->speed_kmh = (float)(atof(spd_s) * 1.852f);
        }
        return true;
    }

    // 2. Phân tích Bản tin GGA (Global Positioning System Fix Data)
    if (gps_is_sentence(fields[0], "GGA")) {
        char fix_q_s[8] = {0};
        char sats_s[8] = {0};
        char hdop_s[12] = {0};

        if (num_fields > 6) strncpy(fix_q_s, fields[6], sizeof(fix_q_s) - 1);
        if (num_fields > 7) strncpy(sats_s, fields[7], sizeof(sats_s) - 1);
        if (num_fields > 8) strncpy(hdop_s, fields[8], sizeof(hdop_s) - 1);

        out->has_gga = true;
        out->fix_quality = (unsigned int)atoi(fix_q_s); // Chất lượng thu nhận tín hiệu
        out->fix_valid = (out->fix_quality > 0);
        out->satellites = (unsigned int)atoi(sats_s);   // Số lượng vệ tinh đang kết nối
        out->hdop = (float)atof(hdop_s);                // Sai số hình học mặt ngang HDOP
        return true;
    }

    // 3. Phân tích Bản tin GSA (GPS DOP and Active Satellites)
    if (gps_is_sentence(fields[0], "GSA")) {
        char fix_type_s[8] = {0};
        char pdop_s[12] = {0};
        char hdop_s[12] = {0};
        char vdop_s[12] = {0};

        if (num_fields > 2) strncpy(fix_type_s, fields[2], sizeof(fix_type_s) - 1);
        if (num_fields > 15) strncpy(pdop_s, fields[15], sizeof(pdop_s) - 1);
        if (num_fields > 16) strncpy(hdop_s, fields[16], sizeof(hdop_s) - 1);
        if (num_fields > 17) strncpy(vdop_s, fields[17], sizeof(vdop_s) - 1);

        out->fix_valid = atoi(fix_type_s) >= 2; // fix type: 1 = no fix, 2 = 2D fix, 3 = 3D fix
        out->pdop = (float)atof(pdop_s);
        out->hdop = (float)atof(hdop_s);
        out->vdop = (float)atof(vdop_s);
        return true;
    }

    return false;
}
