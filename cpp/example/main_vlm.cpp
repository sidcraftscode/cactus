#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <ctime>

#include "utils.h"
#include "../../cpp/cactus.h"

int main(int argc, char **argv) {
    const std::string model_filename = "SmolVLM-256M.gguf";
    const std::string mmproj_filename = "mmproj-SmolVLM-256M.gguf";
    const std::string image_path = "../files/image.jpg";

    const std::string model_url = "https://huggingface.co/ggml-org/SmolVLM-256M-Instruct-GGUF/resolve/main/SmolVLM-256M-Instruct-Q8_0.gguf";
    const std::string mmproj_url = "https://huggingface.co/ggml-org/SmolVLM-256M-Instruct-GGUF/resolve/main/mmproj-SmolVLM-256M-Instruct-Q8_0.gguf";

    if (!downloadFile(model_url, model_filename, "VLM model")) {
        return 1;
    }

    if (!downloadFile(mmproj_url, mmproj_filename, "Multimodal projector")) {
        return 1;
    }

    if (!fileExists(image_path)) {
        std::cerr << "Image file not found: " << image_path << std::endl;
        return 1;
    }

    std::cout << "\n=== Cactus Core API VLM Example ===" << std::endl;

    try {
        cactus::cactus_context context;

        common_params params;
        params.model.path = model_filename;
        params.n_ctx = 2048;
        params.n_batch = 32;
        params.n_gpu_layers = 99;
        params.cpuparams.n_threads = 4;

        std::cout << "Loading model..." << std::endl;
        if (!context.loadModel(params)) {
            std::cerr << "Failed to load model" << std::endl;
            return 1;
        }

        std::cout << "Initializing multimodal..." << std::endl;
        if (!context.initMultimodal(mmproj_filename, true)) {  // Enable GPU acceleration for CLIP
            std::cerr << "Failed to initialize multimodal" << std::endl;
            return 1;
        }

        std::cout << "Vision support: " << (context.isMultimodalSupportVision() ? "Yes" : "No") << std::endl;

        // Function to prompt and get response using proper chat template
        auto prompt_and_respond = [&](const std::string& prompt, const std::vector<std::string>& media_paths = {}, int max_tokens = 50) {
            std::cout << "\n" << std::string(80, '=') << std::endl;
            std::cout << "PROMPT: " << prompt << std::endl;
            if (!media_paths.empty()) {
                std::cout << "MEDIA: " << media_paths.size() << " file(s)" << std::endl;
            }
            std::cout << std::string(80, '-') << std::endl;


            
            // Format the prompt using proper chat template
            std::string formatted_prompt;
            try {
                if (!media_paths.empty()) {
                    std::string messages = R"([{"role": "user", "content": [{"type": "image"}, {"type": "text", "text": ")" + prompt + R"("}]}])";
                    formatted_prompt = context.getFormattedChat(messages, "");
                } else {
                    std::string messages = R"([{"role": "user", "content": [{"type": "text", "text": ")" + prompt + R"("}]}])";
                    formatted_prompt = context.getFormattedChat(messages, "");
                }
            } catch (const std::exception& e) {
                std::cerr << "Warning: Chat template formatting failed (" << e.what() << "), using raw prompt" << std::endl;
                formatted_prompt = prompt;
            }
            
            context.params.prompt = formatted_prompt;
            context.params.n_predict = max_tokens;

            if (!context.initSampling()) {
                std::cerr << "Failed to initialize sampling" << std::endl;
                return false;
            }

        context.rewind();
        context.beginCompletion();
        context.loadPrompt(media_paths);

        while (context.has_next_token && !context.is_interrupted) {
            auto token_output = context.doCompletion();
            if (token_output.tok == -1) break;
        }

            std::cout << "RESPONSE: " << context.generated_text << std::endl;
            
            return true;
        };

        // Test sequence: language, image, language, language, image, image
        std::cout << "\nStarting multi-turn conversation test..." << std::endl;
        if (!prompt_and_respond("Hello! Can you tell me what you are?")) return 1;
        if (!prompt_and_respond("Describe what you see in this image.", {image_path})) return 1;
        if (!prompt_and_respond("What are the main colors you observed?")) return 1;
        if (!prompt_and_respond("Can you write a short poem about vision?")) return 1;
        if (!prompt_and_respond("What emotions or mood does this image convey?", {image_path})) return 1;
        if (!prompt_and_respond("If you had to give this image a title, what would it be?", {image_path})) return 1;
        std::cout << "\nMulti-turn conversation test completed!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}