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
#include <utility>
#include <variant>
#include <vector>

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

        TYPE_COUNT,
        UNKNOWN = TYPE_COUNT,
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

    // Check if the value is a number type (int, float, double, uint32_t etc.)
    template <typename... Types>
    constexpr bool IsNumber(const std::variant<Types...>& v) noexcept {
        return std::visit([]<typename T0>(T0&& arg) {
            using T = std::decay_t<T0>;
            return (std::is_integral_v<T> && !std::is_same_v<T, bool>) || std::is_floating_point_v<T>;
        }, v);
    }

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

        [[nodiscard]] std::string ToString() const {
            return std::visit([](auto&& arg) {
                return std::format("{}", arg);
            }, data);
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
        NodeType type = NodeType::UNKNOWN;

        // Nodes only need to record the next node because they will only be used to turn into an IR, not get executed
        NodeID nextNodeID = INVALID_ID;

        std::vector<PinID> inputPinIDs = std::vector<PinID>();
        std::vector<PinID> outputPinIDs = std::vector<PinID>();

        [[nodiscard]] bool CheckPinCounts(const uint32_t inPinCount, const uint32_t outPinCount) const {
            if (inPinCount != 0 && inputPinIDs.size() != inPinCount) return false;
            if (outPinCount != 0 && outputPinIDs.size() != outPinCount) return false;
            return true;
        }
    };

    struct FlattenedNode {
        NodeID originalNodeID = INVALID_ID; // May not be necessary
        NodeType type = NodeType::UNKNOWN;


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

        void CreateError(const std::string_view error) {
            success = false;
            errorMessage = error;
        }

        // Take a node, strip away the unnecesary information and emit a flattened version for the IR
        [[nodiscard]] FlattenedNode FlattenNode(const Node& node) {
            FlattenedNode output;
            output.originalNodeID = node.id;
            output.type = node.type;

            // Fill in the flattened node's inputs from the original node, the outputs will be created
            // THIS IS COMPLETELY WRONG, THIS HAS TO GO
            output.inputValues.resize(node.inputPinIDs.size());
            for (size_t i = 0; i < node.inputPinIDs.size(); i++) output.inputValues[i] = currentGraph.pins[node.inputPinIDs[i]].value;

            //todo IMPORTANT FILL IN THE FOLLOWING NODE'S INPUTS I AM SICK RIGHT NOW I HAVE TO GO

            auto IsMathOperation = [node, output]() -> bool {
                return node.CheckPinCounts(2, 1) && IsNumber(output.inputValues[0].data) && IsNumber(output.inputValues[1].data);
            };

            switch (node.type) {
                case NodeType::ADD:
                    if (!IsMathOperation()) CreateError("Invalid Pin Count in Node: ADD");
                    output.outputValues[0] = output.inputValues[0] + output.inputValues[1];
                    break;

                case NodeType::SUB:
                    if (!IsMathOperation()) CreateError("Invalid Pin Count in Node: SUB");
                    output.outputValues[0] = output.inputValues[0] - output.inputValues[1];
                    break;

                case NodeType::MUL:
                    if (!IsMathOperation()) CreateError("Invalid Pin Count in Node: MUL");
                    output.outputValues[0] = output.inputValues[0] * output.inputValues[1];
                    break;

                case NodeType::DIV:
                    if (!IsMathOperation()) CreateError("Invalid Pin Count in Node: DIV");
                    output.outputValues[0] = output.inputValues[0] / output.inputValues[1];
                    break;

                case NodeType::TYPE_COUNT:
                default:
                    success = false;
                    errorMessage += "UNKNOWN NODE TYPE";
                    break;
            }

            return output;
        }

        void EmitIR(const FlattenedNode& node) {
            // Example (TEMPORARY)

            auto EmitBinaryOperation = [node](const char op) -> std::string {
                auto first = node.inputValues[0].ToString();
                auto second = node.inputValues[1].ToString();
                return std::format("{} {} {}", first, op, second);
            };

            auto TurnToString = [node, EmitBinaryOperation, this]() -> std::string {
                switch (node.type) {
                    case NodeType::ADD: return EmitBinaryOperation('+');
                    case NodeType::SUB: return EmitBinaryOperation('-');
                    case NodeType::DIV: return EmitBinaryOperation('/');
                    case NodeType::MUL: return EmitBinaryOperation('*');

                    case NodeType::UNKNOWN:
                    default: CreateError("Unknown NodeType while emitting IR"); break;
                }

                return "";
            };

            outputStream << TurnToString();
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