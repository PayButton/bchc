#!/usr/bin/env bash
# Runs a git bisect between two commits based on a specific benchmark, to
# identify the point when a slowdown was introduced.
#
# Specify the one benchmark to run (if a filter matches multiple results, only
# the last benchmark is taken into account, so filter explicitly)  Then specify
# the commit or tag when the benchmark is known to be fast, followed by the one
# when it is known to be slow.
#
# Finally, specify your build directory, as git bisect, and hence this script
# must be run from the repository's root, so we need to know where to build.
#
# Example usage: contrib/bench/bench_bisect.sh BanManAddressIsDiscouraged v24.0.0 v24.1.0 ./build

export LC_ALL=C

benchmark="$1"
fast_commit="$2"
slow_commit="$3"
build_dir="$4"

if [ -z "$build_dir" ]; then
  echo "Usage: $0 benchmark fast_commit slow_commit build_dir" >&2
  echo "Example: $0 BanManAddressIsDiscouraged v24.0.0 v24.1.0 ./build" >&2
  exit 1
fi

# check if we've specified a valid ninja build dir
if [ ! -f "$build_dir/build.ninja" ]; then
  echo "Specify a build directory that has been configured by calling cmake -GNinja.  Usage: $0 benchmark fast_commit slow_commit build_dir" >&2
  exit 1
fi

exit_state_str="Premature exit"

# save the currently checked out branch, and return to it when we're finished
old_branch=$(git branch | grep '^\* ' | cut -d ' ' -f 2-)
# later versions of git have git branch --show-current, but we need to support older versions which don't
function restore_branch {
  if [ ! -z "$old_branch" ]; then
    echo "$exit_state_str, returning to previously checked out branch: $old_branch" >&2
    git checkout "$old_branch"
  else
    # no previous branch to check out (i.e. HEAD was already in a detached state, or similar)
    echo "$exit_state_str, current branch left in undefined state." >&2
  fi
}
# restore the branch even if the benchmark process is interrupted
trap restore_branch EXIT

# if eatmydata is installed locally, we'll use it to call the benchmarks to reduce IO-induced jitter
if which eatmydata >/dev/null; then
  echo "Eatmydata is available in the path - will use it to build and call the benchmarks." >&2
  launch="eatmydata"
else
  launch=""
fi

function get_bench_time {
  echo "Building..." >&2
  cd "$build_dir" || (echo "Unable to enter build directory $build_dir - aborting." >&2; exit 1) || exit 1
  $launch ninja bench_bitcoin >/dev/null 2>&1
  echo "Running $benchmark benchmark..." >&2
  $launch src/bench/bench_bitcoin -filter="$benchmark" | cut -d ' ' -f 4 | cut -d ',' -f 1 | tail -1
}

function time_compare {
  # $2 is < or >, bc outputs "0" or "1" for the comparison result - we then return that as the status code
  (( $(bc <<< "$1 $2 $3") ))
}

# check out the fast commit, and take a benchmark time
echo "Getting baseline times: benchmarking fast commit..."
if ! git -c advice.detachedHead=false checkout "$fast_commit"; then
  echo "Unable to check out $fast_commit - cannot continue."
  exit 1
fi
fast_time=$(get_bench_time)

# check out the slow commit, and take a benchmark time
echo "Getting baseline times: benchmarking slow commit..."
if ! git -c advice.detachedHead=false checkout "$slow_commit"; then
  echo "Unable to checkout $slow_commit - cannot continue."
fi
slow_time=$(get_bench_time)

echo "Benchmark $benchmark runs in ${fast_time}s at $fast_commit, and ${slow_time}s at $slow_commit"

# make sure the fast time is actually faster
if ! time_compare "$fast_time" "<" "$slow_time"; then
  echo "The \"slow\" time is actually faster than the fast time!  Make sure you have the commit order the right way round, and the benchmark is sufficiently repeatable." >&2
  exit 1
fi

time_threshold=$(bc <<< "scale = 4; ($fast_time + $slow_time) / 2")

echo "Confirmed a $(bc <<< "($slow_time * 100 / $fast_time) - 100")% time increase in the $benchmark benchmark."
echo
echo "Searching for the first commit where benchmark time exceeds the halfway threshold of ${time_threshold}s..."
echo
git bisect start --term-old "fast" --term-new "slow" "$slow_commit" "$fast_commit"

while true; do
  bench_time=$(get_bench_time)
  if time_compare "$bench_time" ">" "$time_threshold"; then
    echo "Benchmark time is ${bench_time}s - slower than threshold"
    response=$(git -c color.status=always bisect slow)
  else
    echo "Benchmark time is ${bench_time}s - faster than threshold"
    response=$(git -c color.status=always bisect fast)
  fi
  echo "$response"
  if grep -Fq ' is the first slow commit' <<< "$response"; then
    break
  fi
done

exit_state_str="Benchmark comparison finished"

git bisect reset
