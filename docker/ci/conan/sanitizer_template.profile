{% set compiler, sani = profile_name.split('.') %}

{% set sanitizer_opt_map = {"asan": "address", "tsan": "thread", "ubsan": "undefined"} %}
{% set sanitizer = sanitizer_opt_map[sani] %}

{% set sanitizer_b2_flags_map = {"address": "define=BOOST_USE_ASAN=1 context-impl=ucontext address-sanitizer=on", "thread": "thread-sanitizer=on", "undefined": "undefined-sanitizer=on"} %}
{% set sanitizer_b2_flags_str = sanitizer_b2_flags_map[sanitizer] %}

{% set sanitizer_build_flags_str = "-fsanitize=" ~ sanitizer ~ " -g -O1 -fno-omit-frame-pointer" %}
{% set sanitizer_build_flags = sanitizer_build_flags_str.split(' ') %}
{% set sanitizer_link_flags_str = "-fsanitize=" ~ sanitizer %}
{% set sanitizer_link_flags = sanitizer_link_flags_str.split(' ') %}

include({{ compiler }})

[options]
boost/*:extra_b2_flags="{{ sanitizer_b2_flags_str }}"
boost/*:without_context=False
boost/*:without_stacktrace=True

[conf]
tools.build:cflags+={{ sanitizer_build_flags }}
tools.build:cxxflags+={{ sanitizer_build_flags }}
tools.build:exelinkflags+={{ sanitizer_link_flags }}
tools.build:sharedlinkflags+={{ sanitizer_link_flags }}

tools.info.package_id:confs+=["tools.build:cflags", "tools.build:cxxflags", "tools.build:exelinkflags", "tools.build:sharedlinkflags"]
