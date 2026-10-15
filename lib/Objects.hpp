//
// Created by berke on 10/13/2026.
//

#ifndef GVSL_OBJECTS_H
#define GVSL_OBJECTS_H

#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <oneapi/tbb/partitioner.h>

// Avoid name clashes
namespace GVSL {
    //=======================================
    // Constants
    //=======================================

    template <typename T, typename U>
    auto Add(T a, U b) { return a + b; }

    template <typename T, typename U>
    auto Subtract(T a, U b) { return a - b; }

    // -------
    // Nodes
    // ------
    using NodeID = uint32_t;
    constexpr NodeID INVALID_ID = std::numeric_limits<NodeID>::max();

    enum class NodeType {
        CUSTOM,
        ADD,
        SUB,
        MUL,
        DIV,

        FUNC_START,

        TYPE_COUNT // Will act as the default "unknown" value
    };

    // -------
    // Pins
    // -------

    using PinID = NodeID;

    enum class PinDirection : bool {
        INPUT = true,
        OUTPUT = false,
    };

    // Add more in the future.
    using Storage = std::variant<int, float, bool, double>;

    // Saves having to write unreadable, long lines every time we want to check a type of a pin
    template <typename T, typename... Types>
    constexpr bool IsType(const std::variant<Types...>& v) noexcept { return std::holds_alternative<T>(v); }

    //=======================================
    // Structs and Classes
    //=======================================

    struct Value {
        Storage data;

        // I hate this but I don't know what to replace it with
        friend Value operator+(const Value& lhs, const Value& rhs) {
            return Value{std::visit([](auto a, auto b) -> Storage {
                return a + b;
            }, lhs.data, rhs.data)};
        }

        friend Value operator-(const Value& lhs, const Value& rhs) {
            return Value{std::visit([](auto a, auto b) -> Storage {
                return a - b;
            }, lhs.data, rhs.data)};
        }

        friend Value operator*(const Value& lhs, const Value& rhs) {
            return Value{std::visit([](auto a, auto b) -> Storage {
                return a * b;
            }, lhs.data, rhs.data)};
        }

        friend Value operator/(const Value& lhs, const Value& rhs) {
            return Value{std::visit([](auto a, auto b) -> Storage {
                return a / b;
            }, lhs.data, rhs.data)};
        }
    };

    struct Pin {
        PinID id = INVALID_ID;
        NodeID nodeID = INVALID_ID; // Denormalization but will offer speed-boost

        PinDirection direction = PinDirection::INPUT;

        Value value;
    };

    // A node should only hold the logics, a "Menu Node" should be created to hold the position and other visual things
    struct Node {
        NodeID id = INVALID_ID;
        NodeType type = NodeType::TYPE_COUNT;

        // Nodes only need to record the next node because they will only be used to turn into an IR, not get executed
        NodeID nextNodeID = INVALID_ID;

        std::vector<PinID> inputPinIDs = std::vector<PinID>();
        std::vector<PinID> outputPinIDs = std::vector<PinID>();
    };

    static bool CheckPinCounts(const uint32_t inPinCount, uint32_t outPinCount, const Node& node) {
        if (inPinCount != 0 && node.inputPinIDs.size() != inPinCount) return false;
        if (outPinCount != 0 && node.outputPinIDs.size() != outPinCount) return false;

        return true;
    }

    struct FlattenedNode {
        NodeID originalNodeID = INVALID_ID; // May not be necessary
        NodeType type;


        std::vector<Value> inputValues;
        std::vector<Value> outputValues; // Will be filled by FlattenNode()
    };

    struct Graph {
        std::unordered_map<NodeID, Node> nodes;
        std::unordered_map<PinID, Pin> pins;
    };

    class Compiler {
        std::stringstream outputStream; // Compiled IR

        std::string outputMessage;
        std::string errorMessage;

        bool success = true;

        std::vector<FlattenedNode> flattenedNodes;

        Graph currentGraph;

        // Take a node, strip away the unnecesary information and emit a flattened version for the IR
        [[nodiscard]] FlattenedNode FlattenNode(const Node& node) {
            FlattenedNode output;
            output.originalNodeID = node.id;
            output.type = node.type;

            // Fill in the flattened node's inputs from the original node, the outputs will be created
            for (int i = 0; i < node.inputPinIDs.size(); i++) output.inputValues[i] = currentGraph.pins[node.inputPinIDs[i]].value;

            //todo IMPORTANT FILL IN THE FOLLOWING NODE'S INPUTS I AM SICK RIGHT NOW I HAVE TO GO

            auto CreateError = [this](const std::string_view error) -> void {
                success = false;
                errorMessage = error;
            };

            switch (node.type) {
                case NodeType::ADD:
                    if (!CheckPinCounts(2, 1, node)) CreateError("Invalid Pin Count in Node: ADD");
                    output.inputValues[0] = output.inputValues[0] + output.inputValues[1];
                    break;

                case NodeType::SUB:
                    if (!CheckPinCounts(2, 1, node)) CreateError("Invalid Pin Count in Node: SUB");
                    output.inputValues[0] = output.inputValues[0] - output.inputValues[1];
                    break;

                case NodeType::MUL:
                    if (!CheckPinCounts(2, 1, node)) CreateError("Invalid Pin Count in Node: MUL");
                    output.inputValues[0] = output.inputValues[0] * output.inputValues[1];
                    break;

                case NodeType::DIV:
                    if (!CheckPinCounts(2, 1, node)) CreateError("Invalid Pin Count in Node: DIV");
                    output.inputValues[0] = output.inputValues[0] / output.inputValues[1];
                    break;

                case NodeType::TYPE_COUNT:
                default:
                    success = false;
                    errorMessage += "UNKNOWN NODE TYPE";
                    break;
            }

            return output;
        }

        void EmitIR(FlattenedNode& node) {

        }

    public:
        void Compile(const Graph& graph, NodeID startingNodeID) {
            errorMessage.clear();
            outputMessage.clear();
            outputStream.str("");
            flattenedNodes.clear();
            currentGraph = graph;

            for (const auto& pair : currentGraph.nodes) flattenedNodes.push_back(FlattenNode(pair.second));
            for (FlattenedNode& node : flattenedNodes) EmitIR(node);
        };
    };
}

#endif //GVSL_OBJECTS_H