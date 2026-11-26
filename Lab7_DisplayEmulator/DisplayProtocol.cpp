#include "DisplayProtocol.h"

bool parseCommand(
    const uint8_t* data,
    size_t size,
    Command& command)
{
    if (data == nullptr || size == 0)
        return false;

    auto readU16 = [data](size_t position) -> uint16_t
        {
            return static_cast<uint16_t>(
                data[position] |
                (static_cast<uint16_t>(data[position + 1]) << 8)
                );
        };

    auto readI16 = [&readU16](size_t position) -> int16_t
        {
            return static_cast<int16_t>(readU16(position));
        };

    command = Command{};

    switch (data[0])
    {
    case CLEAR_DISPLAY:
        if (size != 3) return false;

        command.type = CLEAR_DISPLAY;
        command.color = readU16(1);
        return true;

    case DRAW_PIXEL:
        if (size != 7) return false;

        command.type = DRAW_PIXEL;
        command.parameters = {
            readI16(1), readI16(3)
        };
        command.color = readU16(5);
        return true;

    case DRAW_LINE:
        if (size != 11) return false;

        command.type = DRAW_LINE;
        command.parameters = {
            readI16(1), readI16(3),
            readI16(5), readI16(7)
        };
        command.color = readU16(9);
        return true;

    case DRAW_RECTANGLE:
    case FILL_RECTANGLE:
    case DRAW_ELLIPSE:
    case FILL_ELLIPSE:
        if (size != 11) return false;

        command.type = static_cast<CommandType>(data[0]);
        command.parameters = {
            readI16(1), readI16(3),
            readI16(5), readI16(7)
        };
        command.color = readU16(9);
        return true;

    case DRAW_CIRCLE:
    case FILL_CIRCLE:
        if (size != 9) return false;

        command.type = static_cast<CommandType>(data[0]);
        command.parameters = {
            readI16(1), readI16(3), readI16(5)
        };
        command.color = readU16(7);
        return true;

    case DRAW_ROUNDED_RECTANGLE:
    case FILL_ROUNDED_RECTANGLE:
        if (size != 13) return false;

        command.type = static_cast<CommandType>(data[0]);
        command.parameters = {
            readI16(1), readI16(3),
            readI16(5), readI16(7),
            readI16(9)
        };
        command.color = readU16(11);
        return true;

    case DRAW_TEXT:
    {
        if (size < 10) return false;

        uint16_t textLength = readU16(8);

        if (size != static_cast<size_t>(10 + textLength))
            return false;

        command.type = DRAW_TEXT;
        command.parameters = {
            readI16(1), readI16(3)
        };
        command.color = readU16(5);
        command.font = data[7];

        command.text.assign(
            reinterpret_cast<const char*>(data + 10),
            textLength
        );

        return true;
    }

    case DRAW_IMAGE:
    {
        if (size < 9) return false;

        int16_t x = readI16(1);
        int16_t y = readI16(3);
        int16_t w = readI16(5);
        int16_t h = readI16(7);

        if (w <= 0 || h <= 0)
            return false;

        uint64_t imageSize =
            static_cast<uint64_t>(w) *
            static_cast<uint64_t>(h) * 2;

        if (imageSize != size - 9)
            return false;

        command.type = DRAW_IMAGE;
        command.parameters = { x, y, w, h };

        command.imageData.assign(data + 9, data + size);

        return true;
    }

    case SET_ORIENTATION:
        if (size != 2 || data[1] > 3)
            return false;

        command.type = SET_ORIENTATION;
        command.orientation = data[1];
        return true;

    case GET_WIDTH:
        if (size != 1) return false;

        command.type = GET_WIDTH;
        return true;

    case GET_HEIGHT:
        if (size != 1) return false;

        command.type = GET_HEIGHT;
        return true;

    default:
        return false;
    }
}