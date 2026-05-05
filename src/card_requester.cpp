#include "card_requester.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;
using json = nlohmann::json;

CardRequester::CardRequester() {
    // Default setup
    setCacheDirectory("cache");
}

CardRequester::~CardRequester() {
    saveCacheIndex();
}

void CardRequester::setCacheDirectory(const std::string& path) {
    saveCacheIndex(); // Save current index before switching
    
    cache_dir_ = path;
    index_file_ = path + "/index.json";
    
    if (!fs::exists(cache_dir_)) {
        try {
            fs::create_directories(cache_dir_);
        } catch (const std::exception& e) {
            std::cerr << "Failed to create cache directory " << path << ": " << e.what() << "\n";
        }
    }
    
    cache_index_.clear();
    loadCacheIndex();
}

void CardRequester::loadCacheIndex() {
    if (fs::exists(index_file_)) {
        try {
            std::ifstream f(index_file_);
            json j;
            f >> j;
            cache_index_ = j.get<std::map<std::string, std::string>>();
        } catch (const std::exception& e) {
            std::cerr << "Failed to load cache index: " << e.what() << "\n";
        }
    }
}

void CardRequester::saveCacheIndex() {
    if (cache_dir_.empty()) return;
    try {
        std::ofstream f(index_file_);
        if (f.is_open()) {
            json j = cache_index_;
            f << j.dump(4);
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to save cache index: " << e.what() << "\n";
    }
}

std::string CardRequester::sanitizeFilename(const std::string& name) {
    std::string s = name;
    std::replace_if(s.begin(), s.end(), [](char c) {
        return !std::isalnum(static_cast<unsigned char>(c)) && c != ' ' && c != '-';
    }, '_');
    return s;
}

std::string CardRequester::getCachePath(const std::string& card_name, bool back) {
    return cache_dir_ + "/" + sanitizeFilename(card_name) + (back ? "_back.jpg" : "_front.jpg");
}

static sf::Texture* loadOrDownload(const std::string& card_name, const std::string& url, 
                                 const std::string& path, 
                                 std::map<std::string, std::unique_ptr<sf::Texture>>& textures,
                                 const cpr::Header& headers) {
    if (fs::exists(path)) {
        auto tex = std::make_unique<sf::Texture>();
        if (tex->loadFromFile(path)) {
            sf::Texture* ptr = tex.get();
            textures[path] = std::move(tex);
            return ptr;
        }
    }

    if (url.empty()) return nullptr;

    std::cout << "Downloading image for " << card_name << "...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    cpr::Response r = cpr::Get(cpr::Url{url}, headers);
    if (r.status_code == 200) {
        std::ofstream ofs(path, std::ios::binary);
        ofs.write(r.text.data(), r.text.size());
        ofs.close();
        auto tex = std::make_unique<sf::Texture>();
        if (tex->loadFromFile(path)) {
            sf::Texture* ptr = tex.get();
            textures[path] = std::move(tex);
            return ptr;
        }
    } else {
        std::cerr << "Failed to download image: " << r.status_code << "\n";
    }
    return nullptr;
}

CardArt CardRequester::getArt(const std::string& card_name) {
    CardArt art;
    std::string front_path = getCachePath(card_name, false);
    std::string back_path  = getCachePath(card_name, true);

    // Check memory first
    if (textures_.count(front_path)) art.front = textures_[front_path].get();
    if (textures_.count(back_path))  art.back  = textures_[back_path].get();

    if (art.front && (fs::exists(back_path) == (art.back != nullptr))) {
        return art;
    }

    // Try disk next
    if (!art.front && fs::exists(front_path)) {
        auto tex = std::make_unique<sf::Texture>();
        if (tex->loadFromFile(front_path)) {
            art.front = tex.get();
            textures_[front_path] = std::move(tex);
        }
    }
    if (!art.back && fs::exists(back_path)) {
        auto tex = std::make_unique<sf::Texture>();
        if (tex->loadFromFile(back_path)) {
            art.back = tex.get();
            textures_[back_path] = std::move(tex);
        }
    }

    if (art.front && (fs::exists(back_path) == (art.back != nullptr))) {
        return art;
    }

    // Fetch from API
    std::cout << "Fetching card data for: " << card_name << "...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    cpr::Header headers = {{"User-Agent", "mtg-sim/1.0 (Contact: local-dev)"}, {"Accept", "application/json"}};
    cpr::Response r = cpr::Get(cpr::Url{"https://api.scryfall.com/cards/named"},
                               cpr::Parameters{{"fuzzy", card_name}}, headers);

    if (r.status_code == 429) {
        std::cerr << "Rate limited! Waiting 2 seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return getArt(card_name); // Simple retry
    }

    if (r.status_code != 200) return art;

    try {
        json j = json::parse(r.text);
        std::string f_url, b_url;

        if (j.contains("image_uris")) {
            f_url = j["image_uris"]["normal"];
        } else if (j.contains("card_faces")) {
            if (j["card_faces"][0].contains("image_uris")) {
                f_url = j["card_faces"][0]["image_uris"]["normal"];
            }
            if (j["card_faces"].size() > 1 && j["card_faces"][1].contains("image_uris")) {
                b_url = j["card_faces"][1]["image_uris"]["normal"];
            }
        }

        if (!art.front) {
            art.front = loadOrDownload(card_name, f_url, front_path, textures_, headers);
            if (art.front) cache_index_[card_name] = fs::path(front_path).filename().string();
        }
        if (!art.back)  {
            art.back = loadOrDownload(card_name, b_url, back_path,  textures_, headers);
            // We don't really use the back filename in the index currently but we save the index anyway
        }
        
        saveCacheIndex();

    } catch (...) {}

    return art;
}
