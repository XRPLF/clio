{% set compiler, sani = profile_name.split('.') %}

{% set sanitizer_opt_map = {'asan': 'address', 'tsan': 'thread', 'ubsan': 'undefined'} %}
{% set sanitizer = sanitizer_opt_map[sani] %}

include({{ compiler }})

[options]
boost/*:extra_b2_flags="cxxflags=\"-fsanitize={{ sanitizer }}\" linkflags=\"-fsanitize={{ sanitizer }}\""
boost/*:without_stacktrace=True

[conf]
tools.build:cflags+=["-fsanitize={{ sanitizer }}"]
tools.build:cxxflags+=["-fsanitize={{ sanitizer }}"]
tools.build:exelinkflags+=["-fsanitize={{ sanitizer }}"]
