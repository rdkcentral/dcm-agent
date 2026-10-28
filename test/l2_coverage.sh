#!/bin/sh
####################################################################################
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
####################################################################################

# L2 lcov helper. The tree must be built with cov_build.sh (--coverage).
#
#   l2_coverage.sh init     zero counters and capture the lcov baseline
#   l2_coverage.sh ensure   same as init, but only when no baseline exists yet
#   l2_coverage.sh report   capture test data, merge with baseline, filter and
#                           generate $COV_DIR/coverage.info and $COV_DIR/html
#
# gcda counters accumulate on disk, so "report" can run after each L2 script;
# the last run contains the data of every script executed since "init".

SRC_DIR="${top_srcdir:-$(cd "$(dirname "$0")/.." && pwd)}"
COV_DIR="/tmp/l2_coverage"
LCOV_RC="--rc lcov_branch_coverage=1"

mkdir -p "$COV_DIR"

install_lcov() {
    if ! command -v lcov >/dev/null 2>&1; then
        apt-get update && apt-get install -y lcov
    fi
}

cov_init() {
    install_lcov
    lcov --zerocounters --directory "$SRC_DIR"
    lcov --capture --initial --directory "$SRC_DIR" \
         --output-file "$COV_DIR/coverage_base.info" $LCOV_RC
}

cov_report() {
    install_lcov
    lcov --capture --directory "$SRC_DIR" \
         --output-file "$COV_DIR/coverage_test.info" $LCOV_RC

    if [ -f "$COV_DIR/coverage_base.info" ]; then
        lcov --add-tracefile "$COV_DIR/coverage_base.info" \
             --add-tracefile "$COV_DIR/coverage_test.info" \
             --output-file "$COV_DIR/coverage_merged.info" $LCOV_RC
    else
        cp "$COV_DIR/coverage_test.info" "$COV_DIR/coverage_merged.info"
    fi

    # Keep only dcm-agent sources: drop system/dependency headers and test code
    lcov --remove "$COV_DIR/coverage_merged.info" '/usr/*' '*/unittest/*' '*/test/*' \
         --output-file "$COV_DIR/coverage.info" $LCOV_RC

    genhtml "$COV_DIR/coverage.info" \
            --output-directory "$COV_DIR/html" \
            --title "DCM Agent L2 Coverage" \
            --branch-coverage --legend

    lcov --summary "$COV_DIR/coverage.info" $LCOV_RC
    echo "Coverage report : $COV_DIR/html/index.html"
    echo "lcov tracefile  : $COV_DIR/coverage.info"
}

case "$1" in
    init)   cov_init ;;
    ensure) [ -f "$COV_DIR/coverage_base.info" ] || cov_init ;;
    report) cov_report ;;
    *)      echo "Usage: $0 {init|ensure|report}"; exit 1 ;;
esac
