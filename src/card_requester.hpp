#include <SFML/Network.hpp>

class CardRequester {
    public:
        static CardRequester& getInstance() {
            static CardRequester instance;
            return instance;
        }

        CardRequester(CardRequester const&) = delete;
        void operator=(CardRequester const&) = delete;

    private:
        CardRequester() {}
        inline static const std::string HTTP_DOMAIN = "https://api.scryfall.com"; 
};