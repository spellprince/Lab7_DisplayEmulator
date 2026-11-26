#include "GraphicsLibUDP.h"

#include <iostream>
#include <thread>
#include <chrono>

int main()
{
    GraphicsLibUDP display(320, 240);

    if (!display.connectTo("127.0.0.1", 12345))
    {
        std::cout << "Connection error!" << std::endl;
        return 1;
    }

    const uint16_t BLACK = RGB565(10, 18, 32);
    const uint16_t CYAN = RGB565(50, 220, 220);
    const uint16_t WHITE = RGB565(255, 255, 255);
    const uint16_t GREEN = RGB565(70, 230, 150);

    for (uint8_t orientation = 0; orientation < 4; orientation++)
    {
        display.setOrientation(orientation);

        std::this_thread::sleep_for(
            std::chrono::milliseconds(100)
        );

        int width = display.getWidth();
        int height = display.getHeight();

        std::cout << "\nOrientation: "
            << static_cast<int>(orientation) * 90
            << " degrees" << std::endl;

        std::cout << "Width: " << width << std::endl;
        std::cout << "Height: " << height << std::endl;

        display.fillScreen(BLACK);

        display.drawRect(
            5, 5,
            width - 10,
            height - 10,
            CYAN
        );

        display.drawText(
            15, 20,
            "LAB 7",
            WHITE, BLACK, 1
        );

        display.drawText(
            15, 55,
            "ORIENTATION",
            CYAN, BLACK, 1
        );

        display.drawText(
            15, 90,
            orientation == 0 ? "0 DEGREES" :
            orientation == 1 ? "90 DEGREES" :
            orientation == 2 ? "180 DEGREES" :
            "270 DEGREES",
            GREEN, BLACK, 1
        );

        display.drawText(
            15, 130,
            "DISPLAY TEST",
            WHITE, BLACK, 1
        );

        std::this_thread::sleep_for(
            std::chrono::seconds(2)
        );
    }

    display.setOrientation(0);

    std::this_thread::sleep_for(
        std::chrono::milliseconds(200)
    );

    display.disconnect();

    std::cout << "\nTest completed!" << std::endl;

    return 0;
}