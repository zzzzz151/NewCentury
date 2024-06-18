// clang-format off

#pragma once

#include <variant>
#include "3rdparty/ordered_map.h"

template <typename T> struct TunableParam {
    public:
    T value, min, max, step;

    inline TunableParam(T value, T min, T max, T step) 
    : value(value), min(min), max(max), step(step) { }

    // overload function operator
    inline T operator () () const {
        return value;
    }
};

using TunableParamVariant = std::variant<
    TunableParam<i32>*, 
    TunableParam<double>*, 
    TunableParam<float>*
>;

TunableParam<double> UCT_C = TunableParam<double>(1.5, 1.1, 4.0, 0.1);
TunableParam<double> EVAL_SCALE = TunableParam<double>(200, 100, 800, 50);

tsl::ordered_map<std::string, TunableParamVariant> tunableParams = {
    {stringify(UCT_C), &UCT_C},
    {stringify(EVAL_SCALE), &EVAL_SCALE}
};
