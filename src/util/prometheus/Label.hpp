#pragma once

#include <string>
#include <vector>

namespace util::prometheus {

/**
 * @brief Class representing a Prometheus label
 */
class Label {
public:
    /**
     * @brief Construct a new Label object
     *
     * @param name The name of the label
     * @param value The value of the label
     */
    Label(std::string name, std::string value);

    auto
    operator<=>(Label const& rhs) const = default;

    /**
     * @brief Serialize the label to a string in Prometheus format (e.g. name="value"). The value is
     * escaped
     *
     * @return The serialized label
     */
    std::string
    serialize() const;

private:
    std::string name_;
    std::string value_;
};

/**
 * @brief Class representing a collection of Prometheus labels
 */
class Labels {
public:
    Labels() = default;

    /**
     * @brief Construct a new Labels object
     *
     * @param labels The labels to add
     */
    explicit Labels(std::vector<Label> labels);

    /**
     * @brief Serialize the labels to a string in Prometheus format (e.g.
     * {"name1="value1",name2="value2"})
     *
     * @return The serialized labels
     */
    std::string
    serialize() const;

private:
    std::vector<Label> labels_;
};

}  // namespace util::prometheus
