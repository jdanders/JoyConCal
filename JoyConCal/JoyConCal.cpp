#include "stdafx.h"
#include "Windows.h"
#include "hidapi.h"
#include <iostream>
#include <string>
#include <csignal>

// Default right calibration
// deadzone: 174
// RIGHT_X_MIN: 1273  RIGHT_Y_MIN: 1071
// RIGHT_X_MID: 2108  RIGHT_Y_MID: 1869
// RIGHT_X_MAX: 1363  RIGHT_Y_MAX: 1202

#define u8 uint8_t
#define u16 uint16_t

const unsigned short NINTENDO = 1406; // 0x057e
const unsigned short JOYCON_L = 8198; // 0x2006
const unsigned short JOYCON_R = 8199; // 0x2007

// In order to make sure the stick maxes out, scale down measured max
const float SCALE = 0.8; // amount to scale down measured max

#define DATA_BUFFER_SIZE 49
#define OUT_BUFFER_SIZE 49
u8 data[DATA_BUFFER_SIZE];
u16 stick_cal[14];
u16 measured_cal[14];
u16 dead_zone[14];
u8 global_counter[2] = { 0,0 };

hid_device *left_joycon = NULL;
hid_device *right_joycon = NULL;

enum JOYCON_CAL {
    LEFT_X_MAX      =  0,
    LEFT_Y_MAX      =  1,
    LEFT_X_MID      =  2,
    LEFT_Y_MID      =  3,
    LEFT_X_MIN      =  4,
    LEFT_Y_MIN      =  5,
    LEFT_DEAD_ZONE  =  6,
    RIGHT_X_MAX     =  7,
    RIGHT_Y_MAX     =  8,
    RIGHT_X_MID     =  9,
    RIGHT_Y_MID     = 10,
    RIGHT_X_MIN     = 11,
    RIGHT_Y_MIN     = 12,
    RIGHT_DEAD_ZONE = 13,
};

void print_cal(u8 is_left, u16* cal) {
    std::cout << (is_left ? "LEFT_X_MIN: " : "RIGHT_X_MIN: ")
              << (is_left ? cal[LEFT_X_MIN] : cal[RIGHT_X_MIN])
              << (is_left ? "  \tLEFT_Y_MIN: " : " \tRIGHT_Y_MIN: ")
              << (is_left ? cal[LEFT_Y_MIN] : cal[RIGHT_Y_MIN])
              << std::endl;
    std::cout << (is_left ? "LEFT_X_MID: " : "RIGHT_X_MID: ")
              << (is_left ? cal[LEFT_X_MID] : cal[RIGHT_X_MID])
              << (is_left ? "  \tLEFT_Y_MID: " : " \tRIGHT_Y_MID: ")
              << (is_left ? cal[LEFT_Y_MID] : cal[RIGHT_Y_MID])
              << std::endl;
    std::cout << (is_left ? "LEFT_X_MAX: " : "RIGHT_X_MAX: ")
              << (is_left ? cal[LEFT_X_MAX] : cal[RIGHT_X_MAX])
              << (is_left ? "  \tLEFT_Y_MAX: " : " \tRIGHT_Y_MAX: ")
              << (is_left ? cal[LEFT_Y_MAX] : cal[RIGHT_Y_MAX])
              << std::endl;
}

void subcomm(hid_device* joycon, u8* in, u8 len, u8 comm, u8 get_response, u8 is_left)
{
    u8 buf[OUT_BUFFER_SIZE] = { 0 };
    buf[0] = 0x1;
    buf[1] = global_counter[is_left];
    buf[2] = 0x0;
    buf[3] = 0x1;
    buf[4] = 0x40;
    buf[5] = 0x40;
    buf[6] = 0x0;
    buf[7] = 0x1;
    buf[8] = 0x40;
    buf[9] = 0x40;
    buf[10] = comm;
    for (int i = 0; i < len; ++i) {
        buf[11 + i] = in[i];
    }
    if (is_left) {
        if (global_counter[is_left] == 0xf) global_counter[is_left] = 0;
        else ++global_counter[is_left];
    }
    else {
        if (global_counter[is_left] == 0xf) global_counter[is_left] = 0;
        else ++global_counter[is_left];
    }
    hid_write(joycon, buf, OUT_BUFFER_SIZE);
    if (get_response) {
        int n = hid_read_timeout(joycon, data, DATA_BUFFER_SIZE, 50);
    }
}

u8* read_spi(hid_device *jc, u8 addr1, u8 addr2, int len, u8 is_left)
{
    u8 buf[] = { addr2, addr1, 0x00, 0x00, (u8)len };
    int tries = 0;
    do {
        ++tries;
        subcomm(jc, buf, 5, 0x10, 1, is_left);
    } while (tries < 10 && !(data[15] == addr2 && data[16] == addr1));
    return data + 20;
}

void write_cal(hid_device *jc, u8 is_left)
{
    u8 addr1 = 0x80;
    u8 addr2 = is_left ? 0x12 : 0x1d;
    // Choose center point mid dead_zone readings
    measured_cal[is_left ? LEFT_X_MID : RIGHT_X_MID] =
        (dead_zone[is_left ? LEFT_X_MAX : RIGHT_X_MAX]
         + dead_zone[is_left ? LEFT_X_MIN : RIGHT_X_MIN]) /2;
    measured_cal[is_left ? LEFT_Y_MID : RIGHT_Y_MID] =
        (dead_zone[is_left ? LEFT_Y_MAX : RIGHT_Y_MAX]
         + dead_zone[is_left ? LEFT_Y_MIN : RIGHT_Y_MIN]) /2;
    // Make max and min relative to center
    measured_cal[is_left ? LEFT_X_MAX : RIGHT_X_MAX] =
        (measured_cal[is_left ? LEFT_X_MAX : RIGHT_X_MAX]
         - measured_cal[is_left ? LEFT_X_MID : RIGHT_X_MID]) * SCALE;
    measured_cal[is_left ? LEFT_X_MIN : RIGHT_X_MIN] =
        (measured_cal[is_left ? LEFT_X_MID : RIGHT_X_MID]
         - measured_cal[is_left ? LEFT_X_MIN : RIGHT_X_MIN])* SCALE;
    measured_cal[is_left ? LEFT_Y_MAX : RIGHT_Y_MAX] =
        (measured_cal[is_left ? LEFT_Y_MAX : RIGHT_Y_MAX]
         - measured_cal[is_left ? LEFT_Y_MID : RIGHT_Y_MID]) * SCALE;
    measured_cal[is_left ? LEFT_Y_MIN : RIGHT_Y_MIN] =
        (measured_cal[is_left ? LEFT_Y_MID : RIGHT_Y_MID]
         - measured_cal[is_left ? LEFT_Y_MIN : RIGHT_Y_MIN]) * SCALE;
    u8 d0 =      measured_cal[is_left ? LEFT_X_MAX : RIGHT_X_MID] & 0xff;
    u8 d1 = (((  measured_cal[is_left ? LEFT_X_MAX : RIGHT_X_MID] >> 8) & 0x0f)
             | ((measured_cal[is_left ? LEFT_Y_MAX : RIGHT_Y_MID] << 4) & 0xf0));
    u8 d2 = (    measured_cal[is_left ? LEFT_Y_MAX : RIGHT_Y_MID] >> 4) & 0xff;
    u8 d3 =      measured_cal[is_left ? LEFT_X_MID : RIGHT_X_MIN] & 0xff;
    u8 d4 = (((  measured_cal[is_left ? LEFT_X_MID : RIGHT_X_MIN] >> 8) & 0x0f)
             | ((measured_cal[is_left ? LEFT_Y_MID : RIGHT_Y_MIN] << 4) & 0xf0));
    u8 d5 = (    measured_cal[is_left ? LEFT_Y_MID : RIGHT_Y_MIN] >> 4) & 0xff;
    u8 d6 =      measured_cal[is_left ? LEFT_X_MIN : RIGHT_X_MAX] & 0xff;
    u8 d7 = (((  measured_cal[is_left ? LEFT_X_MIN : RIGHT_X_MAX] >> 8) & 0x0f)
             | ((measured_cal[is_left ? LEFT_Y_MIN : RIGHT_Y_MAX] << 4) & 0xf0));
    u8 d8 = (    measured_cal[is_left ? LEFT_Y_MIN : RIGHT_Y_MAX] >> 4) & 0xff;
    u8 buf[] = { addr2, addr1, 0x00, 0x00, 9,
                 d0, d1, d2, d3, d4, d5, d6, d7, d8};

    // To verify above math...
    u16 tmp_cal[14];
    tmp_cal[is_left ? LEFT_X_MAX : RIGHT_X_MID]  = ((d1 << 8) & 0xf00) | d0;
    tmp_cal[is_left ? LEFT_Y_MAX : RIGHT_Y_MID] = ((d2 << 4) | (d1 >> 4));
    tmp_cal[is_left ? LEFT_X_MID : RIGHT_X_MIN] = ((d4 << 8) & 0xf00 | d3);
    tmp_cal[is_left ? LEFT_Y_MID : RIGHT_Y_MIN] = ((d5 << 4) | (d4 >> 4));
    tmp_cal[is_left ? LEFT_X_MIN : RIGHT_X_MAX]  = ((d7 << 8) & 0xf00) | d6;
    tmp_cal[is_left ? LEFT_Y_MIN : RIGHT_Y_MAX]  = ((d8 << 4) | (d7 >> 4));

    int tries = 0;
    do {
        ++tries;
        subcomm(jc, buf, 14, 0x11, 1, is_left);
    } while (tries < 10 && !((data[13] & 0x80) && data[14] == 0x11));
    std::cout << "Wrote calibration:" << std::endl;
    print_cal(is_left, tmp_cal);
    if (data[13] & 0x80) std::cout << "Successfully" << std::endl;
    else std::cout << "Failed" << std::endl;

    // Dead zone
    addr1 = 0x60;
    addr2 = is_left ? 0x86 : 0x98;
    u8 dead_zone_amt = max(
                           dead_zone[is_left ? LEFT_X_MAX : RIGHT_X_MAX] -
                           dead_zone[is_left ? LEFT_X_MIN : RIGHT_X_MIN],
                           dead_zone[is_left ? LEFT_Y_MAX : RIGHT_Y_MAX] -
                           dead_zone[is_left ? LEFT_Y_MIN : RIGHT_Y_MIN]);
    u8 buf2[] = { addr2 + 3, addr1, 0x00, 0x00, 1, dead_zone_amt};
    tries = 0;
    do {
        ++tries;
        subcomm(jc, buf2, 6, 0x11, 1, is_left);
    } while (tries < 10 && !((data[13] & 0x80) && data[14] == 0x11));
    std::cout << "Wrote dead zone:" << (int)dead_zone_amt << std::endl;
    //print_cal(is_left, dead_zone);
    if (data[13] & 0x80) std::cout << "Successfully" << std::endl;
    else std::cout << "Failed" << std::endl;
}

void get_stick_cal(hid_device* jc, u8 is_left)
{
    // dump calibration data
    u8* out = read_spi(jc, 0x80, is_left ? 0x12 : 0x1d, 9, is_left);
    u8 found = 0;
    for (int i = 0; i < 9; ++i) {
        if (out[i] != 0xff) {
            // User calibration data found
            std::cout << "user cal found" << std::endl;
            found = 1;
            break;
        }
    }
    if (!found) {
        std::cout << "User cal not found" << std::endl;
        out = read_spi(jc, 0x60, is_left ? 0x3d : 0x46, 9, is_left);
    }

    stick_cal[is_left ? LEFT_X_MAX : RIGHT_X_MID]  = ((out[1] << 8) & 0xf00) | out[0];
    stick_cal[is_left ? LEFT_Y_MAX : RIGHT_Y_MID] = ((out[2] << 4) | (out[1] >> 4));
    stick_cal[is_left ? LEFT_X_MID : RIGHT_X_MIN] = ((out[4] << 8) & 0xf00 | out[3]);
    stick_cal[is_left ? LEFT_Y_MID : RIGHT_Y_MIN] = ((out[5] << 4) | (out[4] >> 4));
    stick_cal[is_left ? LEFT_X_MIN : RIGHT_X_MAX]  = ((out[7] << 8) & 0xf00) | out[6];
    stick_cal[is_left ? LEFT_Y_MIN : RIGHT_Y_MAX]  = ((out[8] << 4) | (out[7] >> 4));
    out = read_spi(jc, 0x60, is_left ? 0x86 : 0x98, 9, is_left);
    stick_cal[is_left ? LEFT_DEAD_ZONE : RIGHT_DEAD_ZONE] = ((out[4] << 8) & 0xF00 | out[3]);
}

void setup_joycon(hid_device *jc, u8 leds, u8 is_left) {
    // Set Simple HID
    u8 send_buf = 0x3f;
    subcomm(jc, &send_buf, 1, 0x3, 1, is_left);
    // Gather calibration data
    get_stick_cal(jc, is_left);
    // Set LEDs
    send_buf = leds;
    subcomm(jc, &send_buf, 1, 0x30, 1, is_left);
    // Set full poll mode (override Simple HID)
    send_buf = 0x30;
    subcomm(jc, &send_buf, 1, 0x3, 1, is_left);

    std::cout << "Initial calibration values" << std::endl;
    print_cal(is_left, stick_cal);
    std::cout << "deadzone: " << stick_cal[is_left ? LEFT_DEAD_ZONE : RIGHT_DEAD_ZONE] << std::endl;
}

void disconnect_exit() {
    hid_exit();
    exit(0);
}

void measure_stick(bool is_left, uint8_t a, uint8_t b, uint8_t c) {
    u16 raw_x = (uint16_t)(a | ((b & 0xf) << 8));
    u16 raw_y = (uint16_t)((b >> 4) | (c << 4));
    std::cout << "\r" << std::string(80, ' ');
    std::cout << "\rCurrent position: "
              << signed(raw_x) << "\t"
              << signed(raw_y) << "\t"
              << "  Range X: "
              << signed(measured_cal[is_left ? LEFT_X_MIN : RIGHT_X_MIN]) << "-"
              << signed(measured_cal[is_left ? LEFT_X_MAX : RIGHT_X_MAX]) << "\t"
              << "Y: "
              << signed(measured_cal[is_left ? LEFT_Y_MIN : RIGHT_Y_MIN]) << "-"
              << signed(measured_cal[is_left ? LEFT_Y_MAX : RIGHT_Y_MAX]);
    if (raw_x > measured_cal[is_left ? LEFT_X_MAX : RIGHT_X_MAX]) {
        measured_cal[is_left ? LEFT_X_MAX : RIGHT_X_MAX] = raw_x;
    }
    if (raw_x < measured_cal[is_left ? LEFT_X_MIN : RIGHT_X_MIN]) {
        measured_cal[is_left ? LEFT_X_MIN : RIGHT_X_MIN] = raw_x;
    }
    if (raw_y > measured_cal[is_left ? LEFT_Y_MAX : RIGHT_Y_MAX]) {
        measured_cal[is_left ? LEFT_Y_MAX : RIGHT_Y_MAX] = raw_y;
    }
    if (raw_y < measured_cal[is_left ? LEFT_Y_MIN : RIGHT_Y_MIN]) {
        measured_cal[is_left ? LEFT_Y_MIN : RIGHT_Y_MIN] = raw_y;
    }
}

void center_stick(bool is_left, uint8_t a, uint8_t b, uint8_t c) {
    u16 raw_x = (uint16_t)(a | ((b & 0xf) << 8));
    u16 raw_y = (uint16_t)((b >> 4) | (c << 4));
    std::cout << "Current position: "
              << signed(raw_x) << "\t"
              << signed(raw_y) << "\n";
    if (measured_cal[is_left ? LEFT_X_MID : RIGHT_X_MID] == 0){
        // Store initial sample
        measured_cal[is_left ? LEFT_X_MID : RIGHT_X_MID] = raw_x;
        measured_cal[is_left ? LEFT_X_MIN : RIGHT_X_MIN] = raw_x;
        measured_cal[is_left ? LEFT_X_MAX : RIGHT_X_MAX] = raw_x;
        dead_zone[is_left ? LEFT_X_MIN : RIGHT_X_MIN] = raw_x;
        dead_zone[is_left ? LEFT_X_MAX : RIGHT_X_MAX] = raw_x;
    }
    if (measured_cal[is_left ? LEFT_Y_MID : RIGHT_Y_MID] == 0){
        // Store initial sample
        measured_cal[is_left ? LEFT_Y_MID : RIGHT_Y_MID] = raw_y;
        measured_cal[is_left ? LEFT_Y_MIN : RIGHT_Y_MIN] = raw_y;
        measured_cal[is_left ? LEFT_Y_MAX : RIGHT_Y_MAX] = raw_y;
        dead_zone[is_left ? LEFT_Y_MIN : RIGHT_Y_MIN] = raw_y;
        dead_zone[is_left ? LEFT_Y_MAX : RIGHT_Y_MAX] = raw_y;
    }
    // Calculate extremes for deadzone
    if (raw_x > dead_zone[is_left ? LEFT_X_MAX : RIGHT_X_MAX]) {
        dead_zone[is_left ? LEFT_X_MAX : RIGHT_X_MAX] = raw_x;
    }
    if (raw_x < dead_zone[is_left ? LEFT_X_MIN : RIGHT_X_MIN]) {
        dead_zone[is_left ? LEFT_X_MIN : RIGHT_X_MIN] = raw_x;
    }
    if (raw_y > dead_zone[is_left ? LEFT_Y_MAX : RIGHT_Y_MAX]) {
        dead_zone[is_left ? LEFT_Y_MAX : RIGHT_Y_MAX] = raw_y;
    }
    if (raw_y < dead_zone[is_left ? LEFT_Y_MIN : RIGHT_Y_MIN]) {
        dead_zone[is_left ? LEFT_Y_MIN : RIGHT_Y_MIN] = raw_y;
    }
}

bool is_button_pressed() {
    if (data[0] == 0x30 || data[0] == 0x21) {
        // any button but l/r stick button
        return (data[3] | (data[4] & 0xf3) | data[5]);
    }
    return false;
}

void wait_button(hid_device *jc) {
    while (!is_button_pressed()) {
        hid_read(jc, data, DATA_BUFFER_SIZE);
    }
    while (is_button_pressed()) {
        hid_read(jc, data, DATA_BUFFER_SIZE);
    }
}

void process_stick(bool is_left) {
    if (data[0] == 0x30 || data[0] == 0x21) {
        if (is_left) measure_stick(is_left, data[6], data[7], data[8]);
        else measure_stick(is_left, data[9], data[10], data[11]);
    }
}

void process_center(bool is_left) {
    if (data[0] == 0x30 || data[0] == 0x21) {
        if (is_left) center_stick(is_left, data[6], data[7], data[8]);
        else center_stick(is_left, data[9], data[10], data[11]);
    }
}

void joycon_cleanup(hid_device *jc, u8 is_left)
{
    u8 send_buf;
    send_buf = 0xf0; // blink the lights
    subcomm(jc, &send_buf, 1, 0x30, 0, is_left);
    Sleep(20);
    send_buf = 0x01; // Enable low power mode
    subcomm(jc, &send_buf, 1, 0x8, 0, is_left);
    Sleep(20);
    send_buf = 0x00; // disconnect
    subcomm(jc, &send_buf, 1, 0x6, 0, is_left);
    Sleep(20);
    std::cout << (is_left ? "Left" : "Right") << " cleanup done" << std::endl << std::endl;
}

int initialize_left_joycon() {
    struct hid_device_info *left_joycon_info = hid_enumerate(NINTENDO, JOYCON_L);
    if (left_joycon_info != NULL) {
        std::cout << " => found left Joy-Con" << std::endl;
        left_joycon = hid_open(NINTENDO, JOYCON_L, left_joycon_info->serial_number);
        if (left_joycon != NULL) {
            std::cout << " => successfully connected to left Joy-Con" << std::endl;
            setup_joycon(left_joycon, 0x1, 1);
        }
        else {
            std::cout << " => could not connect to left Joy-Con" << std::endl;
            return 1;
        }
    }
    else {
        std::cout << " => could not find left Joy-Con" << std::endl;
        return 1;
    }
    return 0;
}

int initialize_right_joycon() {
    struct hid_device_info *right_joycon_info = hid_enumerate(NINTENDO, JOYCON_R);
    if (right_joycon_info != NULL) {
        std::cout << " => found right Joy-Con" << std::endl;
        right_joycon = hid_open(NINTENDO, JOYCON_R, right_joycon_info->serial_number);
        if (right_joycon != NULL) {
            std::cout << " => successfully connected to right Joy-Con" << std::endl;
            setup_joycon(right_joycon, 0x1, 0);
        }
        else {
            std::cout << " => could not connect to right Joy-Con" << std::endl;
            return 1;
        }
    }
    else {
        std::cout << " => could not find right Joy-Con" << std::endl;
        return 1;
    }
    return 0;
}

void terminate_joyconcal() {
    if (left_joycon) {
        std::cout << "Shutting down left..." << std::endl;
        joycon_cleanup(left_joycon, 1);
    }
    if (right_joycon) {
        std::cout << "Shutting down right..." << std::endl;
        joycon_cleanup(right_joycon, 0);
    }
    std::cout << "disconnecting and exiting..." << std::endl;
    Sleep(1000);
    disconnect_exit();
}

void exit_handler(int signum) {
    terminate_joyconcal();
    Sleep(10000);
    exit(signum);
}

void run_cal(hid_device *joycon, bool is_left){
    // Get initial sample
    std::cout << " => Wiggle the analog stick around a bit, then let go" << std::endl;
    std::cout << " => Leave analog stick centered"
              << " and press a button when ready"
              << std::endl;
    wait_button(joycon);
    while (data[0] != 0x30){
        hid_read(joycon, data, DATA_BUFFER_SIZE);
    }
    process_center(is_left);
    std::cout << " => Recorded initial center position" << std::endl;

    // Measure full range of analog stick
    std::cout << " => Move analog stick to left/right, up/down extremes."
              << " Press button when done." << std::endl;
    while(!is_button_pressed()) {
        hid_read(joycon, data, DATA_BUFFER_SIZE);
        process_stick(is_left);
    }
    std::cout << std::endl;
    wait_button(joycon);

    // Get dead zone
    std::cout << " => Dead Zone measurement: lightly press until you "
              << "feel resistance"
              << std::endl;
    std::cout << " => Lightly press stick up then press a button"
              << std::endl;
    wait_button(joycon);
    while (data[0] != 0x30){
        hid_read(joycon, data, DATA_BUFFER_SIZE);
    }
    process_center(is_left);
    std::cout << " => Lightly press stick down then press a button"
              << std::endl;
    wait_button(joycon);
    while (data[0] != 0x30){
        hid_read(joycon, data, DATA_BUFFER_SIZE);
    }
    process_center(is_left);
    std::cout << " => Lightly press stick left then press a button"
              << std::endl;
    wait_button(joycon);
    while (data[0] != 0x30){
        hid_read(joycon, data, DATA_BUFFER_SIZE);
    }
    process_center(is_left);
    std::cout << " => Lightly press stick right then press a button"
              << std::endl;
    wait_button(joycon);
    while (data[0] != 0x30){
        hid_read(joycon, data, DATA_BUFFER_SIZE);
    }
    process_center(is_left);

    std::cout << "Calibration finished. Raw values:" << std::endl;
    print_cal(is_left, measured_cal);
    std::cout << "Original values:" << std::endl;
    print_cal(is_left, stick_cal);
    write_cal(joycon, is_left);
    joycon_cleanup(joycon, is_left);
}

int main() {
    signal(SIGINT, exit_handler);
    std::cout << "JoyConCal v0.1" << std::endl;
    std::cout << "Connect JoyCon controller(s) and press Enter" << std::endl;
    getchar();

    hid_init();

    // Left
    std::cout << std::endl;
    std::cout << " => (-) left (-) Joy-Con checking" << std::endl;
    if (initialize_left_joycon()) {
        std::cout << " => (-) left (-) Joy-Con not detected" << std::endl;
    } else {
        std::cout << " => Calibrating (-) left (-) Joy-Con" << std::endl;
        run_cal(left_joycon, true);
    }

    // Right
    std::cout << std::endl;
    std::cout << " => (+) right (+) Joy-Con checking" << std::endl;
    if (initialize_right_joycon()) {
        std::cout << " => (+) right (+) Joy-Con not detected" << std::endl;
    } else {
        std::cout << " => Calibrating (+) right (+) Joy-Con" << std::endl;
        run_cal(right_joycon, false);
    }

    std::cout << "Press any key to exit..." << std::endl;
    getchar();
    terminate_joyconcal();
    Sleep(20000);
}
