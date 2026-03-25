#include "util/config/ArrayView.hpp"

#include "util/Assert.hpp"
#include "util/config/Array.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/ObjectView.hpp"
#include "util/config/ValueView.hpp"

#include <cstddef>
#include <string_view>

namespace util::config {

ArrayView::ArrayView(std::string_view prefix, ClioConfigDefinition const& configDef)
    : prefix_{prefix}, clioConfig_{configDef}
{
}

ValueView
ArrayView::valueAt(std::size_t idx) const
{
    ASSERT(
        clioConfig_.get().contains(prefix_),
        "Current string {} is a prefix, not a key of config",
        prefix_
    );
    ConfigValue const& val = clioConfig_.get().asArray(prefix_).at(idx);
    return ValueView{val};
}

size_t
ArrayView::size() const
{
    return clioConfig_.get().arraySize(prefix_);
}

ObjectView
ArrayView::objectAt(std::size_t idx) const
{
    ASSERT(idx < this->size(), "Object index is out of scope");
    return ObjectView{prefix_, idx, clioConfig_};
}

}  // namespace util::config
