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

/**
 * @brief Ensure strncpy did not truncate in the middle of a UTF-8 multibyte sequence.
 *
 * Walks backwards from the end of the null-terminated string; if the last
 * character is the start or continuation of a multibyte sequence that was
 * cut short, truncate at the character boundary.
 */
static void SafeUtf8Truncate(char* buf, int bufSize) {
    int len = static_cast<int>(std::strlen(buf));
    if (len == 0) return;

    // Walk back over continuation bytes (10xxxxxx)
    int pos = len - 1;
    while (pos > 0 && (static_cast<uint8_t>(buf[pos]) & 0xC0) == 0x80) {
        --pos;
    }

    // pos now points to the leading byte of the last character.
    // Determine expected byte count from the leading byte.
    uint8_t lead = static_cast<uint8_t>(buf[pos]);
    int expected = 1;
    if      ((lead & 0xE0) == 0xC0) expected = 2;
    else if ((lead & 0xF0) == 0xE0) expected = 3;
    else if ((lead & 0xF8) == 0xF0) expected = 4;

    // If the character was truncated (not enough bytes), cut it off.
    if (pos + expected > len) {
        buf[pos] = '\0';
    }
}

bool LoadDialogueSequenceFromJSON(const std::string& filepath, DialogueSequence& outSeq) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        LOG_WARN("[DialogueLoader] Cannot open file: " << filepath);
        return false;
    }

    try {
        nlohmann::json root = nlohmann::json::parse(file);

        // delay
        if (root.contains("delay")) {
            outSeq.messageDelay = root["delay"].get<float>();
        }

        outSeq.treeCount = 0;

        // nodes
        if (!root.contains("nodes") || !root["nodes"].is_array()) {
            LOG_WARN("[DialogueLoader] No 'nodes' array in: " << filepath);
            return false;
        }

        outSeq.nodeCount = 0;
        for (auto& nodeJson : root["nodes"]) {
            if (outSeq.nodeCount >= DialogueSequence::kMaxNodes) break;

            auto& node = outSeq.nodes[outSeq.nodeCount];
            memset(&node, 0, sizeof(node));

            // id
            if (nodeJson.contains("id")) {
                std::string s = nodeJson["id"].get<std::string>();
                strncpy(node.id, s.c_str(), sizeof(node.id) - 1);
            }

            // isRoot — collect tree info (treeId + rootNodeId)
            if (nodeJson.contains("isRoot") && nodeJson["isRoot"].get<bool>()) {
                if (outSeq.treeCount < DialogueSequence::kMaxTrees) {
                    auto& tree = outSeq.trees[outSeq.treeCount];
                    strncpy(tree.rootNodeId, node.id, sizeof(tree.rootNodeId) - 1);
                    tree.rootNodeId[sizeof(tree.rootNodeId) - 1] = '\0';
                    // treeId: 从 JSON 读取，若无则回退到 node.id
                    if (nodeJson.contains("treeId")) {
                        std::string s = nodeJson["treeId"].get<std::string>();
                        strncpy(tree.treeId, s.c_str(), sizeof(tree.treeId) - 1);
                    } else {
                        strncpy(tree.treeId, node.id, sizeof(tree.treeId) - 1);
                    }
                    tree.treeId[sizeof(tree.treeId) - 1] = '\0';
                    outSeq.treeCount++;
                }
            }

            // speaker
            if (nodeJson.contains("speaker")) {
                std::string s = nodeJson["speaker"].get<std::string>();
                strncpy(node.speaker, s.c_str(), sizeof(node.speaker) - 1);
                SafeUtf8Truncate(node.speaker, sizeof(node.speaker));
            }

            // message
            if (nodeJson.contains("message")) {
                std::string s = nodeJson["message"].get<std::string>();
                strncpy(node.npcMessage, s.c_str(), sizeof(node.npcMessage) - 1);
                SafeUtf8Truncate(node.npcMessage, sizeof(node.npcMessage));
            }

            // alertDelta (node-level)
            if (nodeJson.contains("alertDelta")) {
                node.alertDelta = nodeJson["alertDelta"].get<float>();
            }

            // timeLimit
            if (nodeJson.contains("timeLimit")) {
                node.replyTimeLimit = nodeJson["timeLimit"].get<float>();
            }

            // waitReply (default true)
            node.waitReply = true;
            if (nodeJson.contains("waitReply")) {
                node.waitReply = nodeJson["waitReply"].get<bool>();
            }

            // isLoop (default false)
            if (nodeJson.contains("isLoop")) {
                node.isLoop = nodeJson["isLoop"].get<bool>();
            }

            // node-level next (auto-advance target)
            if (nodeJson.contains("next")) {
                std::string s = nodeJson["next"].get<std::string>();
                strncpy(node.autoNextId, s.c_str(), sizeof(node.autoNextId) - 1);
            }

            // replies
            if (nodeJson.contains("replies") && nodeJson["replies"].is_array()) {
                node.replyCount = 0;
                for (auto& replyJson : nodeJson["replies"]) {
                    if (node.replyCount >= 4) break;
                    uint8_t ri = node.replyCount;

                    if (replyJson.contains("text")) {
                        std::string s = replyJson["text"].get<std::string>();
                        strncpy(node.replies[ri], s.c_str(), sizeof(node.replies[0]) - 1);
                        SafeUtf8Truncate(node.replies[ri], sizeof(node.replies[0]));
                    }
                    if (replyJson.contains("alertDelta")) {
                        node.replyAlertDelta[ri] = replyJson["alertDelta"].get<float>();
                    }
                    if (replyJson.contains("next")) {
                        std::string s = replyJson["next"].get<std::string>();
                        strncpy(node.nextNodeId[ri], s.c_str(), sizeof(node.nextNodeId[0]) - 1);
                    }
                    // "comment" and "nextSpeaker" are authoring-only, skip

                    node.replyCount++;
                }
            }

            outSeq.nodeCount++;
        }

        if (outSeq.treeCount == 0) {
            LOG_WARN("[DialogueLoader] No isRoot nodes in: " << filepath);
        }

        LOG_INFO("[DialogueLoader] Loaded " << filepath
                 << ": " << outSeq.nodeCount << " nodes, " << outSeq.treeCount << " trees");
        return true;

    } catch (const nlohmann::json::exception& e) {
        LOG_ERROR("[DialogueLoader] JSON parse error in " << filepath << ": " << e.what());
        return false;
    }
}

} // namespace ECS

#ifdef _MSC_VER
#pragma warning(pop)
#endif
