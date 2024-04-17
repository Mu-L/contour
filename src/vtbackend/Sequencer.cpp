// SPDX-License-Identifier: Apache-2.0
#include <vtbackend/Screen.h>
#include <vtbackend/Sequencer.h>
#include <vtbackend/SixelParser.h>
#include <vtbackend/Terminal.h>
#include <vtbackend/logging.h>
#include <vtbackend/primitives.h>

#include <cassert>
#include <string_view>

using namespace std::string_view_literals;

namespace vtbackend
{

Sequencer::Sequencer(Terminal& terminal): _terminal { terminal }, _parameterBuilder { _sequence.parameters() }
{
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void Sequencer::error(std::string_view errorString)
{
    if (vtParserLog)
        vtParserLog()("Parser error: {}", errorString);
}

void Sequencer::print(char32_t codepoint)
{
    if (vtParserLog)
    {
        if (codepoint < 0x80 && std::isprint(static_cast<char>(codepoint)))
            vtParserLog()("Print: '{}'", static_cast<char>(codepoint));
        else
            vtParserLog()("Print: U+{:X}", (unsigned) codepoint);
    }
    _terminal.incrementInstructionCounter();
    _terminal.sequenceHandler().writeText(codepoint);
}

size_t Sequencer::print(std::string_view chars, size_t cellCount)
{
    if (vtParserLog)
        vtParserLog()("Print: ({}) '{}'", cellCount, crispy::escape(chars));

    assert(!chars.empty());

    _terminal.incrementInstructionCounter(chars.size());
    _terminal.sequenceHandler().writeText(chars, cellCount);

    return _terminal.settings().pageSize.columns.as<size_t>()
           - _terminal.currentScreen().cursor().position.column.as<size_t>();
}

void Sequencer::printEnd() noexcept
{
    if (vtParserLog)
        vtParserLog()("PrintEnd");

    _terminal.sequenceHandler().writeTextEnd();
}

void Sequencer::execute(char controlCode)
{
    _terminal.sequenceHandler().executeControlCode(controlCode);
}

void Sequencer::collect(char ch)
{
    _sequence.intermediateCharacters().push_back(ch);
}

void Sequencer::collectLeader(char leader) noexcept
{
    _sequence.setLeader(leader);
}

void Sequencer::param(char ch) noexcept
{
    switch (ch)
    {
        case ';': paramSeparator(); break;
        case ':': paramSubSeparator(); break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': paramDigit(ch); break;
        default: crispy::unreachable();
    }
}

void Sequencer::dispatchESC(char finalChar)
{
    _sequence.setCategory(FunctionCategory::ESC);
    _sequence.setFinalChar(finalChar);
    handleSequence();
}

void Sequencer::dispatchCSI(char finalChar)
{
    _sequence.setCategory(FunctionCategory::CSI);
    _sequence.setFinalChar(finalChar);
    handleSequence();
}

void Sequencer::startOSC() noexcept
{
    _sequence.setCategory(FunctionCategory::OSC);
}

void Sequencer::putOSC(char ch)
{
    if (_sequence.intermediateCharacters().size() + 1 < Sequence::MaxOscLength)
        _sequence.intermediateCharacters().push_back(ch);
}

void Sequencer::dispatchOSC()
{
    auto const [code, skipCount] = vtparser::extractCodePrefix(_sequence.intermediateCharacters());
    _parameterBuilder.set(static_cast<Sequence::Parameter>(code));
    _sequence.intermediateCharacters().erase(0, skipCount);
    handleSequence();
    clear();
}

void Sequencer::hook(char finalChar)
{
    _terminal.incrementInstructionCounter();
    _sequence.setCategory(FunctionCategory::DCS);
    _sequence.setFinalChar(finalChar);

    handleSequence();
}

void Sequencer::put(char ch)
{
    if (_hookedParser)
        _hookedParser->pass(ch);
}

void Sequencer::unhook()
{
    if (_hookedParser)
    {
        _hookedParser->finalize();
        _hookedParser.reset();
    }
}

size_t Sequencer::maxBulkTextSequenceWidth() const noexcept
{
    return _terminal.maxBulkTextSequenceWidth();
}

void Sequencer::handleSequence()
{
    _parameterBuilder.fixiate();
    _terminal.sequenceHandler().processSequence(_sequence);
}

} // namespace vtbackend
