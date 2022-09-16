#ifndef RIPPLE_UTIL_TAGDECORATOR_H
#define RIPPLE_UTIL_TAGDECORATOR_H

#include <boost/json.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <memory>
#include <optional>
#include <ostream>
#include <string>

namespace util {
namespace detail {

/**
 * @brief A `null` tag generator - does nothing.
 */
struct NullTagGenerator final
{
};

/**
 * @brief This strategy uses an `atomic_uint64_t` to remain lock free.
 */
struct UIntTagGenerator final
{
    using tag_t = std::atomic_uint64_t;

    static tag_t
    next();
};

/**
 * @brief This strategy uses `boost::uuids::uuid` with a static random generator
 * and a mutex
 */
struct UUIDTagGenerator final
{
    using tag_t = boost::uuids::uuid;

    static tag_t
    next();
};

}  // namespace detail

/**
 * @brief Represents any tag decorator
 */
class BaseTagDecorator
{
public:
    virtual ~BaseTagDecorator() = default;

    /**
     * @brief Decorates a std::ostream.
     * @param os The stream to decorate
     */
    virtual void
    decorate(std::ostream& os) const = 0;

    /**
     * @brief Support for decorating streams (boost log, cout, etc.).
     *
     * @param os The stream
     * @param decorator The decorator
     * @return std::ostream& The same stream that we were given
     */
    friend std::ostream&
    operator<<(std::ostream& os, BaseTagDecorator const& decorator)
    {
        decorator.decorate(os);
        return os;
    }
};

/**
 * @brief A decorator that decorates a string (log line) with a unique tag.
 * @tparam Generator The strategy used to generate the tag.
 */
template <typename Generator>
class TagDecorator final : public BaseTagDecorator
{
    using parent_t =
        std::optional<std::reference_wrapper<BaseTagDecorator const>>;
    using tag_t = typename Generator::tag_t;

    parent_t parent_ = std::nullopt;
    tag_t tag_ = Generator::next();

public:
    /**
     * @brief Create a new tag decorator with an optional parent
     *
     * If the `parent` is specified it will be streamed out as a chain when this
     * decorator will decorate an ostream.
     *
     * Note that if `parent` is specified it is your responsibility that the
     * decorator referred to by `parent` outlives this decorator.
     *
     * @param parent An optional parent tag decorator
     */
    explicit TagDecorator(parent_t parent = std::nullopt) : parent_{parent}
    {
    }

    /**
     * @brief Implementation of the decoration. Chaining tags when parent is
     * available.
     * @param os The stream to output into
     */
    void
    decorate(std::ostream& os) const override
    {
        os << "[";

        if (parent_.has_value())
            (*parent_).get().decorate(os);

        os << tag_ << "] ";
    }
};

/**
 * @brief Specialization for a nop/null decorator.
 *
 * This generates a pass-thru decorate member function which can be optimized
 * away by the compiler.
 */
template <>
class TagDecorator<detail::NullTagGenerator> final : public BaseTagDecorator
{
public:
    /**
     * @brief Nop implementation for the decorator.
     * @param os The stream
     */
    void
    decorate([[maybe_unused]] std::ostream& os) const override
    {
        // nop
    }
};

/**
 * @brief A factory for TagDecorator instantiation.
 */
class TagDecoratorFactory final
{
    using parent_t =
        std::optional<std::reference_wrapper<BaseTagDecorator const>>;

public:
    /**
     * @brief Represents the type of tag decorator
     */
    enum class Type {
        NONE, /*! No decoration and no tag */
        UUID, /*! Tag based on `boost::uuids::uuid`, thread-safe via mutex */
        UINT  /*! atomic_uint64_t tag, thread-safe, lock-free */
    };

private:
    Type type_; /*! The type of TagDecorator this factory produces */

public:
    ~TagDecoratorFactory() = default;
    TagDecoratorFactory(boost::json::object const& config)
        : type_{TagDecoratorFactory::parseType(config)}
    {
    }

    /**
     * @brief Instantiates the TagDecorator specified by `type_`.
     *
     * Note that if `parent` is specified it is your responsibility that the
     * decorator referred to by `parent` outlives the decorator produced by this
     * factory.
     *
     * @return std::unique_ptr<BaseTagDecorator> An instance of the requested
     * decorator
     */
    std::unique_ptr<BaseTagDecorator>
    make(parent_t parent = std::nullopt) const;

    /**
     * @brief Parses the type of decorator out of the config.
     *
     * @param config The configuration as a json object
     * @return Type The selected type of decorator
     */
    static Type
    parseType(boost::json::object const& config);
};

/**
 * @brief A base class that allows attaching a tag decorator to a subclass.
 */
class Taggable
{
    using decorator_t = std::unique_ptr<util::BaseTagDecorator>;
    decorator_t tagDecorator_;

protected:
    /**
     * @brief New Taggable from a specified factory
     * @param tagFactory The factory to use
     */
    explicit Taggable(util::TagDecoratorFactory const& tagFactory)
        : tagDecorator_{tagFactory.make()}
    {
    }

    /**
     * @brief New Taggable with specific decorator
     * @param decorator The root decorator for this instance
     */
    explicit Taggable(decorator_t decorator)
        : tagDecorator_{std::move(decorator)}
    {
    }

public:
    virtual ~Taggable() = default;

    /**
     * @brief Getter for tag decorator.
     * @return util::BaseTagDecorator const& Reference to the tag decorator
     */
    util::BaseTagDecorator const&
    tag() const
    {
        return *tagDecorator_;
    }
};

}  // namespace util

#endif  // RIPPLE_UTIL_TAGDECORATOR_H
