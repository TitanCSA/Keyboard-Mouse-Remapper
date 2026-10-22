#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace gpr {
inline constexpr int ControlCount=784;
inline constexpr int MouseLeft=768, MouseRight=769, MouseMiddle=770, MouseX1=771, MouseX2=772;
inline constexpr int MouseX=773, MouseY=774, MouseWheel=775, MouseHWheel=776, AbsoluteX=777, AbsoluteY=778;
inline constexpr bool IsMotion(int id) noexcept {return id>=MouseX && id<=AbsoluteY;}
inline constexpr bool IsRelative(int id) noexcept {return id>=MouseX && id<=MouseHWheel;}
inline constexpr bool IsButton(int id) noexcept {return id>=0 && id<MouseX;}
struct ControlFrame {
    std::array<int, ControlCount> Physical{}, Values{};
    std::array<bool, ControlCount> Available{};
    std::uint32_t DeviceKey = 0, Family = 0;
    void CopyPhysicalToOutput() noexcept { Values = Physical; }
};
struct Controls {
    static const std::array<std::string, ControlCount> &Names();
    static int Parse(const std::string &name);
};
class ScriptException : public std::runtime_error {
  public:
    const int Line, Column;
    explicit ScriptException(const std::string &message, int line = 1, int column = 1);
    std::string Diagnostic() const;
};
namespace detail {
struct ProgramData;
}
class ScriptInstance;
enum class ParameterKind { Slider, Toggle, Select, Number };
struct ParameterChoice {
    int Value = 0;
    std::string Label;
};
struct ScriptParameter {
    std::string Name, Label, Group, Help, Unit;
    ParameterKind Kind = ParameterKind::Slider;
    int Minimum = 0, Maximum = 100, Default = 0, Step = 1;
    std::vector<ParameterChoice> Choices;
    bool Accepts(int value) const noexcept;
};
using ParameterValues = std::array<int, 64>;
struct ScriptInterface {
    std::string Title, Description;
    std::vector<ScriptParameter> Parameters;
    ParameterValues Defaults() const noexcept;
    void Validate(const ParameterValues &values) const;
};
class ScriptProgram final {
    std::shared_ptr<const detail::ProgramData> data_;
    explicit ScriptProgram(std::shared_ptr<const detail::ProgramData> data);
    friend struct ScriptCompiler;

  public:
    const std::string &Source() const noexcept;
    const ScriptInterface &Interface() const noexcept;
    std::unique_ptr<ScriptInstance> CreateInstance() const;
};
struct ScriptCompiler {
    static std::shared_ptr<const ScriptProgram> Compile(const std::string &source);
};
class ScriptInstance final {
    struct Impl;
    std::unique_ptr<Impl> impl_;
    explicit ScriptInstance(std::shared_ptr<const detail::ProgramData> program);
    friend class ScriptProgram;

  public:
    ~ScriptInstance();
    ScriptInstance(const ScriptInstance &) = delete;
    ScriptInstance &operator=(const ScriptInstance &) = delete;
    void Execute(ControlFrame &frame, std::int64_t nowMicroseconds, int elapsedMilliseconds);
    void Reset();
    // Only the owning worker changes parameters, between executions.
    void SetParameters(const ParameterValues &values);
    int NextWakeMilliseconds() const noexcept;
    int InstructionBudget() const noexcept;
    void SetInstructionBudget(int value);
};
} // namespace gpr
