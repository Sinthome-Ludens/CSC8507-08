#include "DialogueLoader.h"
#include "Game/Utils/Log.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <cstring>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)  // strncpy deprecation
#endif

namespace ECS {

/// 填充一个 DialogueSequence
static void ParseSequence(const nlohmann::json& arr, DialogueSequence& seq) {
    seq.nodeCount = 0;
    for (auto& nodeJson : arr) {
        if (seq.nodeCount >= DialogueSequence::kMaxNodes) break;

        auto& node = seq.nodes[seq.nodeCount];
        memset(&node, 0, sizeof(node));

        // NPC message
        if (nodeJson.contains("message")) {
            std::string msg = nodeJson["message"].get<std::string>();
            strncpy(node.npcMessage, msg.c_str(), sizeof(node.npcMessage) - 1);
        }

        // Replies
        if (nodeJson.contains("replies") && nodeJson["replies"].is_array()) {
            node.replyCount = 0;
            for (auto& replyJson : nodeJson["replies"]) {
                if (node.replyCount >= 4) break;

                if (replyJson.contains("text")) {
                    std::string txt = replyJson["text"].get<std::string>();
                    strncpy(node.replies[node.replyCount], txt.c_str(),
                            sizeof(node.replies[0]) - 1);
                }
                if (replyJson.contains("effect")) {
                    node.effects[node.replyCount] = (int8_t)replyJson["effect"].get<int>();
                }
                node.replyCount++;
            }
        }

        // Reply time limit
        if (nodeJson.contains("timeLimit")) {
            node.replyTimeLimit = nodeJson["timeLimit"].get<float>();
        }

        seq.nodeCount++;
    }

    // Message delay
    // (set from parent if provided, otherwise keep default)
}

bool LoadDialogueFromJSON(const std::string& filepath, Res_DialogueData& outData) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        LOG_WARN("[DialogueLoader] Cannot open file: " << filepath);
        outData.loaded = false;
        return false;
    }

    try {
        nlohmann::json root = nlohmann::json::parse(file);

        // Proactive
        if (root.contains("proactive")) {
            auto& proObj = root["proactive"];
            if (proObj.contains("delay")) {
                outData.proactive.messageDelay = proObj["delay"].get<float>();
            }
            if (proObj.contains("nodes")) {
                ParseSequence(proObj["nodes"], outData.proactive);
            }
        }

        // Mixed
        if (root.contains("mixed")) {
            auto& mixObj = root["mixed"];
            if (mixObj.contains("delay")) {
                outData.mixed.messageDelay = mixObj["delay"].get<float>();
            }
            if (mixObj.contains("nodes")) {
                ParseSequence(mixObj["nodes"], outData.mixed);
            }
        }

        // Passive
        if (root.contains("passive")) {
            auto& pasObj = root["passive"];
            if (pasObj.contains("delay")) {
                outData.passive.messageDelay = pasObj["delay"].get<float>();
            }
            if (pasObj.contains("nodes")) {
                ParseSequence(pasObj["nodes"], outData.passive);
            }
        }

        outData.loaded = true;
        LOG_INFO("[DialogueLoader] Loaded dialogue: proactive="
                 << outData.proactive.nodeCount << " mixed="
                 << outData.mixed.nodeCount << " passive="
                 << outData.passive.nodeCount);
        return true;

    } catch (const nlohmann::json::exception& e) {
        LOG_ERROR("[DialogueLoader] JSON parse error: " << e.what());
        outData.loaded = false;
        return false;
    }
}

} // namespace ECS

#ifdef _MSC_VER
#pragma warning(pop)
#endif
