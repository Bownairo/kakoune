#ifndef diff_hh_INCLUDED
#define diff_hh_INCLUDED

// Implementation of the linear space variant of the algorithm described in
// "An O(ND) Difference Algorithm and Its Variations"
// (http://xmailserver.org/diff2.pdf)

#include "array_view.hh"
#include "vector.hh"

#include <functional>
#include <iterator>

namespace Kakoune
{

struct Snake{ int x, y, u, v; bool add; };

template<typename Iterator, typename Equal>
Snake find_end_snake_of_further_reaching_dpath(Iterator a, int N, Iterator b, int M,
                                               const int* V, const int D, const int k, Equal eq)
{
    const bool add = k == -D or (k != D and V[k-1] < V[k+1]);

    // if diagonal on the right goes further along x than diagonal on the left,
    // then we take a vertical edge from it to this diagonal, hence x = V[k+1]
    // else, we take an horizontal edge from our left diagonal,x = V[k-1]+1
    const int x = add ? V[k+1] : V[k-1]+1;
    // we are by construction on diagonal k, so our position along b (y) is x - k.
    const int y = x - k;

    int u = x, v = y;
    // follow end snake along diagonal k
    while (u < N and v < M and eq(a[u], b[v]))
        ++u, ++v;

    return { x, y, u, v, add };
}

struct SnakeLen : Snake
{
    SnakeLen(Snake s, bool reverse, int d) : Snake(s), reverse(reverse), d(d) {}
    const bool reverse;
    const int d;
};

template<typename Iterator, typename Equal>
SnakeLen find_middle_snake(Iterator a, int N, Iterator b, int M,
                           int* V1, int* V2, Equal eq)
{
    const int delta = N - M;
    V1[1] = 0;
    V2[1] = 0;

    std::reverse_iterator<Iterator> ra{a + N}, rb{b + M};

    for (int D = 0; D <= (M + N + 1) / 2; ++D)
    {
        for (int k1 = -D; k1 <= D; k1 += 2)
        {
            auto p = find_end_snake_of_further_reaching_dpath(a, N, b, M, V1, D, k1, eq);
            V1[k1] = p.u;

            const int k2 = -(k1 - delta);
            if ((delta % 2 != 0) and -(D-1) <= k2 and k2 <= (D-1))
            {
                if (V1[k1] + V2[k2] >= N)
                    return { p, false, 2 * D - 1 };// return last snake on forward path
            }
        }

        for (int k2 = -D; k2 <= D; k2 += 2)
        {
            auto p = find_end_snake_of_further_reaching_dpath(ra, N, rb, M, V2, D, k2, eq);
            V2[k2] = p.u;

            const int k1 = -(k2 - delta);
            if ((delta % 2 == 0) and -D <= k1 and k1 <= D)
            {
                if (V1[k1] + V2[k2] >= N)
                    return { { N - p.u, M - p.v, N - p.x , M - p.y, p.add } , true, 2 * D };// return last snake on reverse path
            }
        }
    }

    kak_assert(false);
    return { {}, false, 0 };
}

struct Diff
{
    enum { Keep, Add, Remove } mode;
    int len;
    int posB;
};

inline void append_diff(Vector<Diff>& diffs, Diff diff)
{
    if (diff.len == 0)
        return;

    if (not diffs.empty() and diffs.back().mode == diff.mode
        and (diff.mode != Diff::Add or
             diffs.back().posB + diffs.back().len == diff.posB))
        diffs.back().len += diff.len;
    else
        diffs.push_back(diff);
}

template<typename Iterator, typename Equal>
void find_diff_rec(Iterator a, int begA, int endA,
                   Iterator b, int begB, int endB,
                   int* V1, int* V2, Equal eq, Vector<Diff>& diffs)
{
    int prefix_len = 0;
    while (begA != endA and begB != endB and eq(a[begA], b[begB]))
         ++begA, ++begB, ++prefix_len;

    int suffix_len = 0;
    while (begA != endA and begB != endB and eq(a[endA-1], b[endB-1]))
        --endA, --endB, ++suffix_len;

    append_diff(diffs, {Diff::Keep, prefix_len, 0});

    const auto lenA = endA - begA, lenB = endB - begB;

    if (lenA == 0)
        append_diff(diffs, {Diff::Add, lenB, begB});
    else if (lenB == 0)
        append_diff(diffs, {Diff::Remove, lenA, 0});
    else
    {
        auto snake = find_middle_snake(a + begA, lenA, b + begB, lenB, V1, V2, eq);
        kak_assert(snake.d > 0 and snake.u <= lenA and snake.v <= lenB);
        const bool recurse = snake.d > 1;

        if (recurse)
            find_diff_rec(a, begA, begA + snake.x - (snake.reverse ? 0 : (snake.add ? 0 : 1)),
                          b, begB, begB + snake.y - (snake.reverse ? 0 : (snake.add ? 1 : 0)),
                          V1, V2, eq, diffs);

        if (not snake.reverse)
            append_diff(diffs, snake.add ? Diff{Diff::Add, 1, begB + snake.y - 1}
                                         : Diff{Diff::Remove, 1, 0});

        append_diff(diffs, {Diff::Keep, snake.u - snake.x, 0});

        if (snake.reverse)
            append_diff(diffs, snake.add ? Diff{Diff::Add, 1, begB + snake.v}
                                         : Diff{Diff::Remove, 1, 0});

        if (recurse)
            find_diff_rec(a, begA + snake.u + (snake.reverse ? (snake.add ? 0 : 1) : 0), endA,
                          b, begB + snake.v + (snake.reverse ? (snake.add ? 1 : 0) : 0), endB,
                          V1, V2, eq, diffs);
    }

    append_diff(diffs, {Diff::Keep, suffix_len, 0});
}

template<typename Iterator, typename Equal = std::equal_to<typename std::iterator_traits<Iterator>::value_type>>
Vector<Diff> find_diff(Iterator a, int N, Iterator b, int M, Equal eq = Equal{})
{
    const int max = 2 * (N + M) + 1;
    Vector<int> data(2*max);
    Vector<Diff> diffs;
    find_diff_rec(a, 0, N, b, 0, M, &data[N+M], &data[max + N+M], eq, diffs);

    return diffs;
}

}

#endif // diff_hh_INCLUDED
