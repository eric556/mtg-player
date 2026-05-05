#pragma once
#include <SFML/Graphics.hpp>
extern "C" {
    #include <clay.h>
}
#include <vector>
#include <iostream>

class ClaySFMLRenderer {
public:
    ClaySFMLRenderer(sf::RenderWindow& window, const sf::Font& font) 
        : window_(window), font_(font) {}

    void render(Clay_RenderCommandArray commands) {
        for (uint32_t i = 0; i < commands.length; ++i) {
            Clay_RenderCommand* cmd = Clay_RenderCommandArray_Get(&commands, i);
            
            switch (cmd->commandType) {
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                    auto data = cmd->renderData.rectangle;
                    sf::RectangleShape rect({cmd->boundingBox.width, cmd->boundingBox.height});
                    rect.setPosition({cmd->boundingBox.x, cmd->boundingBox.y});
                    rect.setFillColor(sf::Color(data.backgroundColor.r, data.backgroundColor.g, data.backgroundColor.b, data.backgroundColor.a));
                    window_.draw(rect);
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                    auto data = cmd->renderData.text;
                    sf::Text text(font_, std::string(data.stringContents.chars, data.stringContents.length), data.fontSize);
                    text.setPosition({cmd->boundingBox.x, cmd->boundingBox.y});
                    text.setFillColor(sf::Color(data.textColor.r, data.textColor.g, data.textColor.b, data.textColor.a));
                    window_.draw(text);
                    break;
                }
                default:
                    break;
            }
        }
    }

private:
    sf::RenderWindow& window_;
    const sf::Font& font_;
};
