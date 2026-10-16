//
// Created by berke on 10/13/2026.
//

#ifndef GVSL_OBJECTS_H
#define GVSL_OBJECTS_H

#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <ranges>
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

    enum class PinSourceType : uint8_t {
        LITERAL,
        REFERENCE
    };

    // Variable types supported
    using Storage = std::variant<int, float, bool, double, std::monostate, std::string>;

    // Saves having to write unreadable, long lines every time we want to check a type of a pin
    template<typename T, typename... Types>
    constexpr bool IsType(const std::variant<Types...> &v) noexcept { return std::holds_alternative<T>(v); }

    // Check if the value is a number type (int, float, double, uint32_t etc.)
    template<typename... Types>
    constexpr bool IsNumber(const std::variant<Types...> &v) noexcept {
        return std::visit([]<typename T0>(T0 &&arg) {
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
        friend Value operator+(const Value &lhs, const Value &rhs) {
            if (!lhs.IsValid() || !rhs.IsValid()) return Value::Invalid();

            return Value{
                std::visit([](auto a, auto b) -> Storage {
                    using T1 = std::decay_t<decltype(a)>;
                    using T2 = std::decay_t<decltype(b)>;

                    // Allow math if both are numbers (int, float, double, bool)
                    if constexpr (std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>) return a + b;
                    // Allow string concatenation if both are strings
                    else if constexpr (std::is_same_v<T1, std::string> && std::is_same_v<T2, std::string>) return a + b;
                    else return std::monostate{}; // Invalid combination

                }, lhs.data, rhs.data)
            };
        }

        friend Value operator-(const Value &lhs, const Value &rhs) {
            if (!lhs.IsValid() || !rhs.IsValid()) return Value::Invalid();

            return Value{
                std::visit([](auto a, auto b) -> Storage {
                    using T1 = std::decay_t<decltype(a)>;
                    using T2 = std::decay_t<decltype(b)>;

                    if constexpr (std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>) return a - b;
                    else return std::monostate{};

                }, lhs.data, rhs.data)
            };
        }

        friend Value operator*(const Value &lhs, const Value &rhs) {
            if (!lhs.IsValid() || !rhs.IsValid()) return Value::Invalid();

            return Value{
                std::visit([](auto a, auto b) -> Storage {
                    using T1 = std::decay_t<decltype(a)>;
                    using T2 = std::decay_t<decltype(b)>;

                    if constexpr (std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>) return a * b;
                    else return std::monostate{};
                }, lhs.data, rhs.data)
            };
        }

        friend Value operator/(const Value &lhs, const Value &rhs) {
            if (!lhs.IsValid() || !rhs.IsValid()) return Value::Invalid();

            return Value{
                std::visit([](auto a, auto b) -> Storage {
                    using T1 = std::decay_t<decltype(a)>;
                    using T2 = std::decay_t<decltype(b)>;

                    if constexpr (std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>) {
                        // Optional: Handle division by zero here
                        return a / b;
                    } else return std::monostate{};

                }, lhs.data, rhs.data)
            };
        }

        [[nodiscard]] std::string ToString() const {
            return std::visit([]<typename T0>(T0 &&arg) -> std::string {
                using T = std::decay_t<T0>;

                if constexpr (std::is_same_v<T, std::monostate>) return "null";
                else if constexpr (std::is_same_v<T, bool>) return arg ? "true" : "false";
                else return std::format("{}", arg);
            }, data);
        }

        // Helper checks
        [[nodiscard]] bool IsValid() const {
            // It's valid if it is NOT std::monostate
            return !std::holds_alternative<std::monostate>(data);
        }

        static Value Invalid() {
            return Value{std::monostate{}};
        }
    };

    struct Pin {
        PinID id = INVALID_ID;
        NodeID nodeID = INVALID_ID; // Denormalization but will offer speed-boost
        PinSourceType type; // if type == REFERENCE, the value refers to a user variable, not a string literal

        std::vector<PinID> nextPinIDs;

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
        NodeType type = NodeType::UNKNOWN;

        std::vector<Value> inputValues;
    };

    struct Graph {
        std::unordered_map<NodeID, Node> nodes;
        std::unordered_map<PinID, Pin> pins;

        std::unordered_map<std::string, Value> variables;

        // first -> second
        void ConnectPin(const PinID first, const PinID second) { pins[first].nextPinIDs.push_back(second); }

        Value GetVariableValue(const std::string &varName) {
            auto it = variables.find(varName);
            if (it != variables.end()) return it->second;

            return Value::Invalid();
        }
    };

    class Compiler {
        std::stringstream outputStream; // Compiled IR

        std::string outputMessage;
        std::string errorMessage;

        bool success = true;

        std::vector<FlattenedNode> flattenedNodes;

        Graph currentGraph;

        std::vector<NodeID> evaluationOrder; // nodes in the order they were evaluated

        Value GetVariableValueSafe(const std::string &varName) {
            Value v = currentGraph.GetVariableValue(varName);

            if (v.IsValid()) return v;

            CreateError("Undeclared variable name");
            return Value{};
        }

        void CreateError(const std::string_view error) {
            success = false;
            errorMessage = error;
        }

        // Compute one node's outputs from its inputs and push them to connected pins
        bool EvaluateNode(const Node &node) {
            std::vector<Value> inputValues(node.inputPinIDs.size());
            for (size_t i = 0; i < node.inputPinIDs.size(); i++)
                inputValues[i] = currentGraph.pins[node.inputPinIDs[i]].value;

            std::vector<Value> outputValues(node.outputPinIDs.size());

            auto IsMathOperationNode = [&]() -> bool {
                return node.CheckPinCounts(2, 1) && IsNumber(inputValues[0].data) && IsNumber(inputValues[1].data);
            };

            switch (node.type) {
                case NodeType::ADD:
                case NodeType::SUB:
                case NodeType::MUL:
                case NodeType::DIV: {
                    // Check the pin counts first so we never index past the end of inputValues
                    if (!node.CheckPinCounts(2, 1)) {
                        CreateError("Invalid Pin Count in math Node");
                        return false;
                    }
                    if (!IsMathOperationNode()) {
                        CreateError("Non-number input in math Node");
                        return false;
                    }

                    const Value &a = inputValues[0];
                    const Value &b = inputValues[1];

                    // Integer division by zero is undefined behaviour, floats just give inf
                    if (node.type == NodeType::DIV && IsType<int>(b.data) && std::get<int>(b.data) == 0) {
                        CreateError("Integer division by zero");
                        return false;
                    }

                    switch (node.type) {
                        case NodeType::ADD: outputValues[0] = a + b;
                            break;
                        case NodeType::SUB: outputValues[0] = a - b;
                            break;
                        case NodeType::MUL: outputValues[0] = a * b;
                            break;
                        default: outputValues[0] = a / b;
                            break;
                    }
                    break;
                }

                case NodeType::FUNC_START: break; // Entry point, nothing to compute

                case NodeType::CUSTOM: CreateError("CUSTOM nodes are not implemented yet");
                    return false;
                case NodeType::UNKNOWN:
                default: CreateError("Unknown Node Type");
                    return false;
            }

            // Store the results on the output pins, then forward them to every connected input pin
            for (size_t i = 0; i < node.outputPinIDs.size(); i++) {
                Pin &outputPin = currentGraph.pins[node.outputPinIDs[i]];
                outputPin.value = outputValues[i];
                for (PinID p: outputPin.nextPinIDs) currentGraph.pins[p].value = outputPin.value;
            }

            return true;
        }

        // Go through every single node, calculate the output pins and populate the following node's inputs
        void PopulatePins() {
            evaluationOrder.clear();

            // Which nodes feed which inputs
            std::unordered_map<PinID, NodeID> sourceOf;
            for (const auto &pin: currentGraph.pins | std::views::values)
                for (PinID next: pin.nextPinIDs) sourceOf[next] = pin.nodeID;

            enum class State { UNVISITED, VISITING, DONE };
            std::unordered_map<NodeID, State> state;

            std::function<bool(NodeID)> Visit = [&](const NodeID id) -> bool {
                State &s = state[id];
                if (s == State::DONE) return true;
                if (s == State::VISITING) {
                    CreateError("Cycle detected in graph");
                    return false;
                }
                s = State::VISITING;

                const auto nodeIt = currentGraph.nodes.find(id);
                if (nodeIt == currentGraph.nodes.end()) {
                    CreateError("Pin refers to missing Node");
                    return false;
                }

                const Node &node = nodeIt->second;

                // Evaluate everything feeding this node first
                for (PinID in: node.inputPinIDs)
                    if (auto it = sourceOf.find(in); it != sourceOf.end() && !Visit(it->second)) return false;

                if (!EvaluateNode(node)) return false;

                s = State::DONE;
                evaluationOrder.push_back(id);
                return true;
            };

            for (const auto &id: currentGraph.nodes | std::views::keys) if (!Visit(id)) return;
        }

        // Take a node, strip away the unnecessary information and emit a flattened version for the IR
        [[nodiscard]] FlattenedNode FlattenNode(const Node &node) {
            FlattenedNode output;
            output.type = node.type;

            // Fill in the flattened node's inputs from the original node, the outputs will be created
            output.inputValues.resize(node.inputPinIDs.size());
            for (size_t i = 0; i < node.inputPinIDs.size(); i++)
                output.inputValues[i] = currentGraph.pins[node.inputPinIDs[i]].value;

            return output;
        }

        void EmitIR(const FlattenedNode &node) {
            // Example (TEMPORARY)

            auto EmitBinaryOperation = [node](const char op) -> std::string {
                auto first = node.inputValues[0].ToString();
                auto second = node.inputValues[1].ToString();
                return std::format("{} {} {}", first, op, second);
            };

            auto TurnToString = [node, EmitBinaryOperation, this]() -> std::string {
                switch (node.type) {
                    case NodeType::ADD: return EmitBinaryOperation('+');
                    // for example this will emit "a + b", this is just a test example
                    case NodeType::SUB: return EmitBinaryOperation('-');
                    case NodeType::DIV: return EmitBinaryOperation('/');
                    case NodeType::MUL: return EmitBinaryOperation('*');

                    case NodeType::UNKNOWN:
                    default: CreateError("Unknown NodeType while emitting IR");
                        break;
                }

                return "";
            };

            outputStream << TurnToString() << '\n';
        }

    public:
        void Compile(const Graph &graph, NodeID startingNodeID) {
            errorMessage.clear();
            outputMessage.clear();
            outputStream.str("");
            flattenedNodes.clear();
            currentGraph = graph;
            success = true;

            PopulatePins();
            if (!success) return;

            for (NodeID id: evaluationOrder) flattenedNodes.push_back(FlattenNode(currentGraph.nodes[id]));
            for (FlattenedNode &node: flattenedNodes) EmitIR(node);
        };

        [[nodiscard]] std::string GetOutput() const { return outputStream.str(); }
        [[nodiscard]] const std::string &GetError() const { return errorMessage; }
        [[nodiscard]] bool Succeeded() const { return success; }
    };
}

#endif //GVSL_OBJECTS_H
