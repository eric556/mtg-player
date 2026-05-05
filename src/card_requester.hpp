#pragma once
#include <string>
#include <map>
#include <memory>
#include <SFML/Graphics.hpp>

struct CardArt {
    sf::Texture* front = nullptr;
    sf::Texture* back  = nullptr;
};

class CardRequester {
public:
    static CardRequester& getInstance() {
        static CardRequester instance;
        return instance;
    }

    CardRequester(const CardRequester&) = delete;
    void operator=(const CardRequester&) = delete;

    // Configures the cache directory. Should be called before any getArt() calls.
    void setCacheDirectory(const std::string& path);

    // Fetches the 'normal' images for a card by name.
    // Handles multi-faced cards by returning both front and back.
    CardArt getArt(const std::string& card_name);

private:
    CardRequester();
    ~CardRequester();

    void loadCacheIndex();
    void saveCacheIndex();

    std::string getCachePath(const std::string& card_name, bool back = false);
    std::string sanitizeFilename(const std::string& name);

    std::map<std::string, std::unique_ptr<sf::Texture>> textures_;
    std::map<std::string, std::string> cache_index_; // name -> local filename
    std::string cache_dir_ = "cache";
    std::string index_file_ = "cache/index.json";
};
