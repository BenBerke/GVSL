//
// Created by berke on 10/13/2026.
//

#ifndef GVSL_OBJECTS_H
#define GVSL_OBJECTS_H

#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Avoid name clashes
namespace GVSL {
    using NodeID = uint32_t;
    using PinID = NodeID;
    constexpr NodeID INVALID_ID = std::numeric_limits<NodeID>::max();

    enum class NodeType {
        CUSTOM,
        ADD,
        SUB,
        MUL,
        DIV,
    };

    // A node should only hold the logics, a "Menu Node" should be created to hold the position and other visual things
    class Node {
    public:
        NodeID id = INVALID_ID;
        NodeType type;

        // Nodes only need to record the next node because they will only be used to turn into an IR, not get executed
        PinID nextNodeID = INVALID_ID;

        std::vector<PinID> inputPinIDs = std::vector<PinID>();
        std::vector<PinID> outputPinIDs = std::vector<PinID>();

        explicit Node(const NodeType t) : type(t) { }
        virtual ~Node() = default; // Needed for inheritance
    };

    // A pin is an input / output to a Node
    enum PinDirection { Input, Output };
    class Pin {
    public:
        std::string name;

        PinID id = INVALID_ID;
        NodeID nodeID = INVALID_ID; // Denormalization but will offer speed-boost

        PinDirection direction = Input;

        std::vector<PinID> nextPinIDs = std::vector<PinID>();

        Pin(std::string n, const PinDirection d) : direction(d), name(std::move(n)) {}
        virtual ~Pin() = default; // Needed for inheritance
    };

    class Graph {
        NodeID nextNodeID = 0;
        PinID nextPinID = 0;

        std::unordered_map<NodeID, std::unique_ptr<Node>> nodes;
        std::unordered_map<PinID, std::unique_ptr<Pin>> pins;

    public:
        template<typename T = Node, typename... Args>
        NodeID CreateNode(Args&&... args) {
            NodeID id = nextNodeID++;

            auto node = std::make_unique<T>(std::forward<Args>(args)...);
            node->id = id;

            nodes[id] = std::move(node);
            return id;
        }

        template<typename T = Pin, typename... Args>
        PinID CreatePin(NodeID nodeID, PinDirection direction, Args&&... args) {
            const PinID id = nextPinID++;

            auto pin = std::make_unique<T>(std::forward<Args>(args)...);
            pin->id = id;
            pin->nodeId = nodeID;
            pin->direction = direction;

            auto it = nodes.find(nodeID);
            if (it != nodes.end()) {
                if (direction == Input) it->second->inputPinIDs.push_back(id);
                else it->second->outputPinIDs.push_back(id);
            }

            pins[id] = std::move(pin);
            return id;
        }

        Node* GetNode(const NodeID id) {
            auto it = nodes.find(id);
            if (it != nodes.end()) return it->second.get();
            return nullptr;
        }

        Pin* GetPin(const PinID id) {
            auto it = pins.find(id);
            if (it != pins.end()) return it->second.get();
            return nullptr;
        }
    };



    class Compiler {
        static constexpr std::string_view error_InvalidID = "Invalid Node ID";

        bool success = true;
        std::string errorMessage;
        std::string outputMessage;
    public:
        bool Compile(Graph& graph, NodeID startNodeID) {
            std::unordered_set<NodeID> visited;

            TraverseNode(graph, startNodeID, visited);

            if (!success) {
                outputMessage = "Compilation Failed";
                if (!errorMessage.empty()) outputMessage += errorMessage;
            }
            else outputMessage = "Compilation Success";

            return success;
        }
    private:
        void SetErrorMessage(std::string_view message, NodeID nodeID) {
            errorMessage = std::format("[ERROR] ", message, " Node ID: ", nodeID);
        }

        void TraverseNode(Graph& graph, NodeID currentNodeID, std::unordered_set<NodeID>& visited) {
            if (currentNodeID == INVALID_ID) {
                SetErrorMessage(error_InvalidID, currentNodeID);
                success = false;
                return;
            }

            if (visited.contains(currentNodeID)) return;

            visited.insert(currentNodeID);

            Node* node = graph.GetNode(currentNodeID);
            if (!node) return;

            // Resolve data dependencies
            for (const PinID pinID : node->inputPinIDs) {
                const Pin* pin = graph.GetPin(pinID);
                if (!pin || pin->direction != Input) continue;

                // If this input pin is connected to an output pin from another node:
                for (const PinID connectedOutputPinID : pin->nextPinIDs) {
                    Pin* outPin = graph.GetPin(connectedOutputPinID);
                    if (outPin) TraverseNode(graph, outPin->nodeID, visited);
                }
            }

            EmitIR(node);

            if (node->nextNodeID != INVALID_ID) TraverseNode(graph, node->nextNodeID, visited);
        }

        // This is just a placeholder, currently not enough information is being passed to emit anything.
        void EmitIR(Node* node) {
            //todo
        }
    };
}

#endif //GVSL_OBJECTS_H