/* Bộ phân tích cú pháp chuỗi dữ liệu GPS NMEA 0183. */

#ifndef GPS_PARSER_H
#define GPS_PARSER_H

#include <stdbool.h>

/* Cấu trúc chứa dữ liệu định vị GPS sau khi phân tích cú pháp NMEA */
typedef struct {
    bool has_rmc;             // Đã nhận và phân tích cú pháp bản tin RMC thành công
    bool has_gga;             // Đã nhận và phân tích cú pháp bản tin GGA thành công
    bool fix_valid;           // Trạng thái định vị GPS hợp lệ (đã khóa vệ tinh)
    double latitude_deg;      // Vĩ độ quy đổi (độ thập phân)
    double longitude_deg;     // Kinh độ quy đổi (độ thập phân)
    float speed_kmh;          // Tốc độ di chuyển (km/h)
    unsigned int fix_quality; // Chất lượng tín hiệu định vị (0: không vị trí, 1: GPS thường, 2: DGPS, v.v.)
    unsigned int satellites;  // Số lượng vệ tinh đang kết nối lấy dữ liệu
    float pdop;               // Sai số vị trí không gian (Position Dilution of Precision)
    float hdop;               // Sai số vị trí mặt phẳng ngang (Horizontal Dilution of Precision)
    float vdop;               // Sai số vị trí chiều dọc (Vertical Dilution of Precision)
} gps_data_t;

/* Kiểm tra tính hợp lệ của chuỗi NMEA thông qua mã Checksum ở cuối dòng tin (sau dấu *)
   - line: Chuỗi dòng tin NMEA cần kiểm tra
   Trả về: true nếu Checksum khớp */
bool gps_checksum_valid(const char *line);

/* Phân tích cú pháp một dòng tin NMEA (RMC, GGA, GSA) và cập nhật dữ liệu định vị
   - line: Chuỗi dòng tin NMEA thô
   - out: Cấu trúc chứa dữ liệu định vị cập nhật
   Trả về: true nếu dòng tin được phân tích cú pháp thành công */
bool gps_parse_line(char *line, gps_data_t *out);

#endif /* GPS_PARSER_H */
