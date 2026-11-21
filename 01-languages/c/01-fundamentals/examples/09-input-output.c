#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== Standard I/O & Formatting Demo ===\n");
    
    /* snprintf bounds-checked formatting */
    char buffer[32];
    int bytes_needed = snprintf(buffer, sizeof(buffer), "DeviceID: %04X, State: %s", 0x42A, "READY");
    printf("Formatted buffer: '%s' (Bytes needed: %d, Buffer capacity: %zu)\n", 
           buffer, bytes_needed, sizeof(buffer));

    /* Simulated safe line parsing via sscanf */
    const char *csv_line = "SensorAlpha,24.5,1013";
    char name[16];
    float temp;
    int pressure;
    int matched = sscanf(csv_line, "%15[^,],%f,%d", name, &temp, &pressure);
    printf("Parsed %d fields -> Sensor: %s, Temp: %.1f C, Pressure: %d hPa\n",
           matched, name, (double)temp, pressure);

    return 0;
}
