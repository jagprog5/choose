#!/bin/bash
rm -f /usr/bin/choose /usr/bin/choose_injector /usr/bin/choose_injector_wrapper.bash ~/.choose.bash
sed -i '/[ -f ~\/.choose.bash ] && source ~\/.choose.bash/d' ~/.bashrc
