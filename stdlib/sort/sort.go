// Tiny subset of sort: insertion sort of []int and []string; Slice is heapsort.
package sort

func Ints(x []int) {
	n := len(x)
	for i := 1; i < n; i++ {
		v := x[i]
		j := i
		for j > 0 && x[j-1] > v {
			x[j] = x[j-1]
			j--
		}
		x[j] = v
	}
}

func Strings(x []string) {
	n := len(x)
	for i := 1; i < n; i++ {
		v := x[i]
		j := i
		for j > 0 && x[j-1] > v {
			x[j] = x[j-1]
			j--
		}
		x[j] = v
	}
}

func IntsAreSorted(x []int) bool {
	for i := 1; i < len(x); i++ {
		if x[i] < x[i-1] {
			return false
		}
	}
	return true
}

func StringsAreSorted(x []string) bool {
	for i := 1; i < len(x); i++ {
		if x[i] < x[i-1] {
			return false
		}
	}
	return true
}

func Float64s(x []float64) {
	n := len(x)
	for i := 1; i < n; i++ {
		v := x[i]
		j := i
		for j > 0 && x[j-1] > v {
			x[j] = x[j-1]
			j--
		}
		x[j] = v
	}
}

func Float64sAreSorted(x []float64) bool {
	for i := 1; i < len(x); i++ {
		if x[i] < x[i-1] {
			return false
		}
	}
	return true
}

type Interface interface {
	Len() int
	Less(i int, j int) bool
	Swap(i int, j int)
}

func Sort(data Interface) {
	n := data.Len()
	for i := 1; i < n; i++ {
		for j := i; j > 0 && data.Less(j, j-1); j-- {
			data.Swap(j, j-1)
		}
	}
}

func IsSorted(data Interface) bool {
	n := data.Len()
	for i := 1; i < n; i++ {
		if data.Less(i, i-1) {
			return false
		}
	}
	return true
}

// Slice/SliceStable are generic (`[]T`), not reflection-based like real
// Go's `sort.Slice(x any, ...)` -- this compiler has no reflection to swap
// through an `any`. The common `sort.Slice(xs, func(i, j int) bool {...})`
// idiom still works: `xs` and `x` share one backing array (see README's
// Rosetta table on wasigo::Slice), so sorting `x` in place reorders the
// closure's `xs` too.
//
// Slice is heapsort: O(n log n) and, like real Go's, not stable.
// SliceStable is insertion sort: stable, O(n^2).
func Slice[T any](x []T, less func(i int, j int) bool) {
	n := len(x)
	for i := n/2 - 1; i >= 0; i-- {
		siftDown(x, less, i, n)
	}
	for end := n - 1; end > 0; end-- {
		x[0], x[end] = x[end], x[0]
		siftDown(x, less, 0, end)
	}
}

// siftDown restores the max-heap below root within x[:n].
func siftDown[T any](x []T, less func(i int, j int) bool, root int, n int) {
	for {
		child := 2*root + 1
		if child >= n {
			return
		}
		if child+1 < n && less(child, child+1) {
			child++
		}
		if !less(root, child) {
			return
		}
		x[root], x[child] = x[child], x[root]
		root = child
	}
}

func SliceStable[T any](x []T, less func(i int, j int) bool) {
	n := len(x)
	for i := 1; i < n; i++ {
		for j := i; j > 0 && less(j, j-1); j-- {
			x[j], x[j-1] = x[j-1], x[j]
		}
	}
}

func SliceIsSorted[T any](x []T, less func(i int, j int) bool) bool {
	for i := 1; i < len(x); i++ {
		if less(i, i-1) {
			return false
		}
	}
	return true
}

func Search(n int, f func(int) bool) int {
	lo := 0
	hi := n
	for lo < hi {
		mid := (lo + hi) / 2
		if !f(mid) {
			lo = mid + 1
		} else {
			hi = mid
		}
	}
	return lo
}

func SearchInts(x []int, target int) int {
	return Search(len(x), func(i int) bool { return x[i] >= target })
}

func SearchStrings(x []string, target string) int {
	return Search(len(x), func(i int) bool { return x[i] >= target })
}

func SearchFloat64s(x []float64, target float64) int {
	return Search(len(x), func(i int) bool { return x[i] >= target })
}
