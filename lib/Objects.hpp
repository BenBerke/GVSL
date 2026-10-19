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
#include <unordered_set>
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

        FUNC_DEC,
        FUNC_END,

        TYPE_COUNT,
        UNKNOWN = TYPE_COUNT,
    };

    // -------
    // Pins
    // -------
    // A pin is an input / output field of a Node

    using PinID = NodeID;

    enum class PinDirection : bool {
        INPUT = true,
        OUTPUT = false,
    };

    // Reference means Value is a string of a variable name. Literal is a literal value like 5.0
    enum class PinSourceType : uint8_t {
        LITERAL,
        REFERENCE
    };

    // Variable types supported
    using Storage = std::variant<int, float, bool, double, std::monostate, std::string>;

    // Check which type of value the pin holds (int, float, string etc.)
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

    // The value a pin holds
    struct Value {
        Storage data; // Can be an int, a float, a double etc.

        // Operator overloading to support pin.value + pin.value
        friend Value operator+(const Value &lhs, const Value &rhs) {
            if (!lhs.IsValid() || !rhs.IsValid()) return Value::Invalid();

            return Value{
                std::visit([](const auto& a, auto b) -> Storage {
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
                        // Optional: Handle division by zero here too (compiler already does it)
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
        PinSourceType sourceType; // if type == REFERENCE, the value refers to a user variable, not a string literal

        std::vector<PinID> nextPinIDs; // If this is an output pin, this is a list of every single input pins it connects to

        PinDirection direction = PinDirection::INPUT;

        Value value;
    };

    // Payload struct
    struct FunctionDecData {
        std::string name;
        uint8_t parameterCount;
        std::vector<Value> parameters;
    };

    // Payload is the extra, type-specific information a node has to carry.
    // For example, a function declaration node would have to carry a function name and a paremeter count while a for-loop has to carry the loop count etc.
    using Payload = std::variant<FunctionDecData>;

    // A node should only hold the logics, a "Menu Node" should be created to hold the position and other visual things
    struct Node {
        NodeID id = INVALID_ID;
        NodeType type = NodeType::UNKNOWN;

        Payload payload;

        // Nodes only need to record the next node because they will only be used to turn into an IR, not get executed
        NodeID nextNodeID = INVALID_ID;

        std::vector<PinID> inputPinIDs = std::vector<PinID>();
        std::vector<PinID> outputPinIDs = std::vector<PinID>();

        // Returns true if the node has the specified amount of input and output pins, false otherwise
        [[nodiscard]] bool CheckPinCounts(const uint32_t inPinCount, const uint32_t outPinCount) const {
            if (inPinCount != 0 && inputPinIDs.size() != inPinCount) return false;
            if (outPinCount != 0 && outputPinIDs.size() != outPinCount) return false;
            return true;
        }
    };

    // Stripped version of a Node that only contains the necessary information to emit an IR
    struct FlattenedNode {
        NodeType type = NodeType::UNKNOWN;

        std::vector<Value> inputValues;

        std::optional<Payload> payload;
    };

    // Manages pins and nodes
    struct Graph {
        std::unordered_map<NodeID, Node> nodes;
        std::unordered_map<PinID, Pin> pins;

        std::unordered_map<std::string, Value> variables;

        NodeID nextNodeID = 0;
        PinID nextPinID = 0;

        // first -> second
        void ConnectPin(const PinID first, const PinID second) { pins[first].nextPinIDs.push_back(second); }

        Value GetVariableValue(const std::string &varName) {
            auto it = variables.find(varName);
            if (it != variables.end()) return it->second;

            return Value::Invalid();
        }

        bool CreateVariable(const std::string &varName, const Value &value) {
            auto it = variables.find(varName);
            if (it != variables.end()) return false; // Variable with same name exists

            variables[varName] = value;
            return true;
        }

        // Create and register a new node
        Node& CreateNode(const NodeType type) {
            const NodeID id = nextNodeID++;

            Node& node = nodes[id];
            node.id = id;
            node.type = type;

            return node;
        }

        // Create and register a new pin
        Pin& CreatePin(const PinDirection direction, const NodeID ownerNodeID) {
            const PinID id = nextPinID++;

            Pin& pin = pins[id];
            pin.id = id;
            pin.nodeID = ownerNodeID;
            pin.direction = direction;

            // Automatically link this pin to its parent node's pin lists
            Node& ownerNode = nodes[ownerNodeID];
            if (direction == PinDirection::INPUT) ownerNode.inputPinIDs.push_back(id);
            else ownerNode.outputPinIDs.push_back(id);

            return pin;
        }

        [[nodiscard]] Node* GetNode(const NodeID id) {
            auto it = nodes.find(id);
            return (it != nodes.end()) ? &it->second : nullptr;
        }

        [[nodiscard]] const Node* GetNode(const NodeID id) const {
            auto it = nodes.find(id);
            return (it != nodes.end()) ? &it->second : nullptr;
        }

        [[nodiscard]] Pin* GetPin(const PinID id) {
            auto it = pins.find(id);
            return (it != pins.end()) ? &it->second : nullptr;
        }

        [[nodiscard]] const Pin* GetPin(const PinID id) const {
            auto it = pins.find(id);
            return (it != pins.end()) ? &it->second : nullptr;
        }
    };

    class Compiler {
        std::stringstream outputStream; // Compiled IR

        std::string outputMessage; // For warnings, errors, succeses etc.
        std::string errorMessage;

        bool success = true;

        std::vector<FlattenedNode> flattenedNodes;

        Graph currentGraph;

        // Which node feeds each input pin, built once per compile
        std::unordered_map<PinID, NodeID> sourceOf;

        // Every node that sits on a function's exec chain
        std::unordered_set<NodeID> execNodes;

        enum class State { UNVISITED, VISITING, DONE };
        std::unordered_map<NodeID, State> state;

        // Saves having to type graph.GetVariableValue() everytime, also can return an invalid node
        Value GetVariableValueSafe(const std::string &varName) {
            Value v = currentGraph.GetVariableValue(varName);

            if (v.IsValid()) return v;

            RaiseError(std::format("Undeclared variable name: {}", varName));
            return Value::Invalid();
        }

        void RaiseError(const std::string_view error) {
            success = false;
            errorMessage = error;
        }

        // Compute one node's outputs from its inputs and push them to connected pins
        bool EvaluateNode(const Node &node) {
            std::vector<Value> outputValues(node.outputPinIDs.size());
            std::vector<Value> calculatedInputValues(node.inputPinIDs.size());

            for (size_t i = 0; i < node.inputPinIDs.size(); i++) {
                Pin* p = currentGraph.GetPin(node.inputPinIDs[i]);

                if (p == nullptr) {
                    RaiseError("Evaluation failed: Invalid pin ID encountered.");
                    return false;
                }

                if (p->sourceType == PinSourceType::LITERAL) calculatedInputValues[i] = p->value;
                else if (p->sourceType == PinSourceType::REFERENCE) {
                    calculatedInputValues[i] = GetVariableValueSafe(p->value.ToString());
                    if (!calculatedInputValues[i].IsValid()) return false;
                    // error already set, no need to set again
                }
            }

            // Ensure the node has 2 input pins, 1 output pin and input pins hold a number value
            auto IsMathOperationNode = [&]() -> bool {
                return node.CheckPinCounts(2, 1) && IsNumber(calculatedInputValues[0].data) &&
                    IsNumber(calculatedInputValues[1].data);
            };

            switch (node.type) {
                // These binary operators have near identical conditions, so just repeat the same case
                case NodeType::ADD:
                case NodeType::SUB:
                case NodeType::MUL:
                case NodeType::DIV: {
                    // Check the pin counts first so we never index past the end of inputValues
                    if (!node.CheckPinCounts(2, 1)) {
                        RaiseError("Invalid Pin Count in math Node");
                        return false;
                    }
                    if (!IsMathOperationNode()) {
                        RaiseError("Non-number input in math Node");
                        return false;
                    }

                    const Value &a = calculatedInputValues[0];
                    const Value &b = calculatedInputValues[1];

                    // Integer division by zero is undefined behaviour, floats just give inf
                    if (node.type == NodeType::DIV && IsType<int>(b.data) && std::get<int>(b.data) == 0) {
                        RaiseError("Integer division by zero");
                        return false;
                    }

                    // Calculate the output value of the node
                    switch (node.type) {
                        case NodeType::ADD: outputValues[0] = a + b; break;
                        case NodeType::SUB: outputValues[0] = a - b; break;
                        case NodeType::MUL: outputValues[0] = a * b; break;
                        case NodeType::DIV: outputValues[0] = a / b; break;
                        default: break;
                    }
                    break;
                }

                case NodeType::FUNC_DEC: break; // Entry point, nothing to compute
                case NodeType::FUNC_END: break; // End point, nothing to compute

                case NodeType::CUSTOM: RaiseError("CUSTOM nodes are not implemented yet"); return false;
                case NodeType::UNKNOWN:
                default: RaiseError("Unknown Node Type"); return false;
            }

            // Store the results on the output pins, then forward them to every connected input pin
            for (size_t i = 0; i < node.outputPinIDs.size(); i++) {
                Pin *outputPin = currentGraph.GetPin(node.outputPinIDs[i]);
                if (outputPin == nullptr) continue;

                outputPin->value = outputValues[i];

                for (const PinID pID: outputPin->nextPinIDs) {
                    Pin *targetPin = currentGraph.GetPin(pID);
                    if (targetPin == nullptr) continue;

                    targetPin->value = outputPin->value;
                    targetPin->sourceType = PinSourceType::LITERAL;
                }
            }

            return true;
        }

        // Create an unordered_map that stores which output pins are connected to which input pins
        void BuildSourceMap() {
            sourceOf.clear();
            for (const auto &pin: currentGraph.pins | std::views::values) for (PinID next: pin.nextPinIDs) sourceOf[next] = pin.nodeID;
        }

        // Follow every FUNC_DEC node's nextNodeID chain, one line per function
        bool CollectFunctionLines(std::vector<std::vector<NodeID>> &functionLines) {
            execNodes.clear();

            // Collect the ids first, creating FUNC_END nodes below can rehash the map we would be iterating
            std::vector<NodeID> funcDecIDs;
            // Look at every node and find which ones are function declarations
            for (const Node &val: currentGraph.nodes | std::views::values)
                if (val.type == NodeType::FUNC_DEC) funcDecIDs.push_back(val.id);

            // Loop through all the function declarations and collect the nodes
            for (const NodeID funcDecID: funcDecIDs) {
                const Node &val = currentGraph.nodes[funcDecID];

                std::vector<NodeID> currentLine;
                std::unordered_set<NodeID> seen;

                // Create a vector of nodes starting from this FUNC_DEC node
                const Node* currentlyVisiting = currentGraph.GetNode(val.nextNodeID);
                currentLine.push_back(val.id);
                while (currentlyVisiting != nullptr) {
                    if (!seen.insert(currentlyVisiting->id).second) {
                        RaiseError("Cycle detected in function exec chain");
                        return false;
                    }

                    currentLine.push_back(currentlyVisiting->id);
                    execNodes.insert(currentlyVisiting->id);
                    currentlyVisiting = currentGraph.GetNode(currentlyVisiting->nextNodeID);
                }

                Node& n = currentGraph.CreateNode(NodeType::FUNC_END);
                currentLine.push_back(n.id);
                functionLines.push_back(currentLine);
            }

            return true;
        }

        // Evaluate a node after everything feeding it, appending to order
        bool Visit(const NodeID id, std::vector<NodeID> &order) {
            State &s = state[id];
            if (s == State::DONE) return true;
            if (s == State::VISITING) {
                RaiseError("Cycle detected in graph");
                return false;
            }
            s = State::VISITING;

            const Node* node = currentGraph.GetNode(id);
            if (node == nullptr) {
                RaiseError("Evaluation failed: Invalid node ID encountered.");
                return false;
            }

            // Evaluate everything feeding this node first, only pure nodes may be pulled in out of exec order
            for (PinID in: node->inputPinIDs) {
                auto it = sourceOf.find(in);
                if (it == sourceOf.end()) continue;

                if (execNodes.contains(it->second) && state[it->second] != State::DONE) {
                    RaiseError("Value used before its node is executed");
                    return false;
                }
                if (!Visit(it->second, order)) return false;
            }

            if (!EvaluateNode(*node)) return false;

            state[id] = State::DONE; // Re-lookup, the recursion may have rehashed the map
            order.push_back(id);
            return true;
        }

        // Walk one function's exec chain, pulling each node's data inputs first
        bool EvaluateLine(const std::vector<NodeID> &line, std::vector<NodeID> &order) {
            state.clear(); // Pure nodes get re-evaluated per function
            for (NodeID id: line) if (!Visit(id, order)) return false;
            return true;
        }

        // Take a node, strip away the unnecessary information and emit a flattened version for the IR
        [[nodiscard]] FlattenedNode FlattenNode(const Node &node) {
            FlattenedNode output;
            output.type = node.type;

            // Fill in the flattened node's inputs from the original node, the outputs will be created
            output.inputValues.resize(node.inputPinIDs.size());
            for (size_t i = 0; i < node.inputPinIDs.size(); i++)
                output.inputValues[i] = currentGraph.pins[node.inputPinIDs[i]].value;

            output.payload = node.payload;

            return output;
        }

        // ============================================
        //                    IR
        // ===========================================-

        std::string EmitFunctionIR(const FlattenedNode& node) {
            const auto funcData = std::get<FunctionDecData>(node.payload.value());

            std::string funcName = funcData.name;

            return std::format("void {}({})", funcName, funcData.parameterCount);
        }

        void EmitIR(const FlattenedNode &node) {
            // Example (TEMPORARY)

            // Helper function, returns a string such as "a + b" where 'a' and 'b' are Node's inputs
            auto EmitBinaryOperation = [node](const char op) -> std::string {
                auto first = node.inputValues[0].ToString();
                auto second = node.inputValues[1].ToString();
                return std::format("{} {} {}", first, op, second);
            };

            // The heart of the IR, turns nodes into string
            auto TurnToString = [node, EmitBinaryOperation, this]() -> std::string {
                switch (node.type) {
                    case NodeType::ADD: return EmitBinaryOperation('+');
                    // for example this will emit "a + b", this is just a test example
                    case NodeType::SUB: return EmitBinaryOperation('-');
                    case NodeType::DIV: return EmitBinaryOperation('/');
                    case NodeType::MUL: return EmitBinaryOperation('*');

                    case NodeType::FUNC_DEC: return EmitFunctionIR(node);
                    case NodeType::FUNC_END: return "}";

                    case NodeType::UNKNOWN:
                    default: RaiseError("Unknown NodeType while emitting IR");
                        break;
                }

                return "";
            };

            outputStream << TurnToString() << '\n';
        }

    public:
        void Compile(const Graph &graph) {
            // Reset compile to default state
            errorMessage.clear();
            outputMessage.clear();
            outputStream.str("");
            flattenedNodes.clear();
            currentGraph = graph;
            success = true;

            // Connect each output pin to an input pin for evaluation
            BuildSourceMap();

            // Look at every single function declaration and create a vector for each chain of nodes starting from the function declaration
            std::vector<std::vector<NodeID>> functionLines;
            if (!CollectFunctionLines(functionLines)) return;

            for (const std::vector<NodeID> &line: functionLines) {
                std::vector<NodeID> order;
                if (!EvaluateLine(line, order)) return;

                for (NodeID id: order) flattenedNodes.push_back(FlattenNode(currentGraph.nodes[id]));
            }

            for (FlattenedNode &node: flattenedNodes) EmitIR(node);
        };

        [[nodiscard]] std::string GetOutput() const { return outputStream.str(); }
        [[nodiscard]] const std::string& GetError() const { return errorMessage; }
        [[nodiscard]] bool Succeeded() const { return success; }
    };
}

#endif //GVSL_OBJECTS_H
