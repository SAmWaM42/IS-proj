#include <iostream>
#include <string>
#include <array>

#include <json.hpp>

int main() {
    const std::string python_executable = "/home/samuel/Documents/School/4.2/IS-2/wordTokenizer/bin/python";
    const std::string tokenizer_script = "/home/samuel/Documents/School/4.2/IS-2/wordTokenizer/Tokenizer/tokens.py";
    const std::string request = "{\"text\":\"I like fetch.\"}";

    std::string command = "printf '%s\\n' '{\"text\":\"I like fetch.\"}' | \"" + python_executable + "\" \"" + tokenizer_script + "\"";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to launch tokenizer process." << std::endl;
        return 1;
    }

    std::string response;
    std::array<char, 512> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        response += buffer.data();
    }

    const int exit_code = pclose(pipe);
    if (exit_code != 0) {
        std::cerr << "Tokenizer process failed with exit code: " << exit_code << std::endl;
        std::cerr << response << std::endl;
        return 2;
    }

    if (response.empty()) {
        std::cerr << "No response received from tokenizer process." << std::endl;
        return 3;
    }

    try {
        const nlohmann::json parsed = nlohmann::json::parse(response);

        if (!parsed.contains("tokens") || !parsed["tokens"].is_array()) {
            std::cerr << "Tokenizer response missing 'tokens' array." << std::endl;
            std::cerr << parsed.dump(2) << std::endl;
            return 4;
        }

        const auto& tokens = parsed["tokens"];
        if (tokens.empty()) {
            std::cerr << "Tokenizer response returned an empty token list." << std::endl;
            return 5;
        }

        std::cout << "Tokenizer validation passed." << std::endl;
        std::cout << parsed.dump(2) << std::endl;

        const std::string first_token = tokens[0].at("text").get<std::string>();
        if (first_token != "I") {
            std::cerr << "Unexpected first token: " << first_token << std::endl;
            return 6;
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse tokenizer JSON response: " << e.what() << std::endl;
        std::cerr << response << std::endl;
        return 7;
    }
}
