#include "vigine/graph/abstractgraph.h"

#include <shared_mutex>
#include <string>

namespace vigine::graph
{
namespace
{
// Emits a small integer into a string buffer without pulling in locale or
// stream overhead. The exporter runs under a shared lock; keeping it
// allocation-light matters for larger graphs.
void appendUint(std::string &out, std::uint64_t value)
{
    if (value == 0)
    {
        out.push_back('0');
        return;
    }
    char   buf[21];
    size_t len = 0;
    while (value > 0 && len < sizeof(buf))
    {
        buf[len++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }
    for (size_t i = 0; i < len; ++i)
    {
        out.push_back(buf[len - 1 - i]);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// exportGraphViz — writes a minimal DOT graph into the caller-supplied
// buffer. No file-system access; the caller owns the string and is free to
// route it anywhere. The exporter prints each live node with its kind tag
// and every live edge as a directed arrow between the two endpoints.
// ---------------------------------------------------------------------------

Result AbstractGraph::exportGraphViz(std::string &out) const
{
    std::shared_lock lock(_mutex);
    out.clear();
    out += "digraph G {\n";
    for (const auto &[index, slot] : _nodes)
    {
        if (!slot.node)
        {
            continue;
        }
        out += "  n";
        appendUint(out, index);
        out += " [label=\"n";
        appendUint(out, index);
        out += " kind=";
        appendUint(out, static_cast<std::uint64_t>(slot.node->kind()));
        out += "\"];\n";
    }
    for (const auto &[index, slot] : _edges)
    {
        if (!slot.edge)
        {
            continue;
        }
        out += "  n";
        appendUint(out, slot.edge->from().index);
        out += " -> n";
        appendUint(out, slot.edge->to().index);
        out += ";\n";
    }
    out += "}\n";
    return Result();
}

} // namespace vigine::graph
