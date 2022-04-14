
#define FZY_SCORE_MAX f32_INF
#define FZY_SCORE_MIN -f32_INF
#define FZY_SCORE_GAP_LEADING -0.005
#define FZY_SCORE_GAP_TRAILING -0.005
#define FZY_SCORE_GAP_INNER -0.01
#define FZY_SCORE_MATCH_CONSECUTIVE 1.0
#define FZY_SCORE_MATCH_SLASH 0.9
#define FZY_SCORE_MATCH_WORD 0.8
#define FZY_SCORE_MATCH_CAPITAL 0.7
#define FZY_SCORE_MATCH_DOT 0.6

#define FZY_SCORE_EPSILON 0.001

using fzy_score_t = f64;

static fzy_score_t fzy_bonus_states[3][256];
static size_t fzy_bonus_index[256];

#define fzy_compute_bonus(last_ch, ch) \
    (fzy_bonus_states[fzy_bonus_index[(unsigned char)(ch)]][(unsigned char)(last_ch)])

void fzy_init_table()
{
    memset(fzy_bonus_index, 0, sizeof fzy_bonus_index);
    memset(fzy_bonus_states, 0, sizeof fzy_bonus_states);

    for (i32 i = 'A'; i < 'Z'; i++) {
        fzy_bonus_index[i] = 2;
    }

    for (i32 i = 'a'; i < 'z'; i++) {
        fzy_bonus_index[i] = 1;
        fzy_bonus_states[2][i] = FZY_SCORE_MATCH_CAPITAL;
    }

    for (i32 i = '0'; i < '9'; i++) {
        fzy_bonus_index[i] = 1;
    }

    fzy_bonus_states[1]['/'] = fzy_bonus_states[2]['/'] = FZY_SCORE_MATCH_SLASH;
    fzy_bonus_states[1]['\\'] = fzy_bonus_states[2]['\\'] = FZY_SCORE_MATCH_SLASH;
    fzy_bonus_states[1]['-'] = fzy_bonus_states[2]['-'] = FZY_SCORE_MATCH_WORD;
    fzy_bonus_states[1]['_'] = fzy_bonus_states[2]['_'] = FZY_SCORE_MATCH_WORD;
    fzy_bonus_states[1][' '] = fzy_bonus_states[2][' '] = FZY_SCORE_MATCH_WORD;
    fzy_bonus_states[1]['.'] = fzy_bonus_states[2]['.'] = FZY_SCORE_MATCH_DOT;
}

void quicksort(Array<fzy_score_t> scores, Array<String> values, i32 l, i32 r)
{
    if (l < 0 || r < 0 || l >= r) return;

    fzy_score_t pivot = scores[(r+l)/2];

    i32 i = l-1;
    i32 j = r+1;

    i32 pi = -1;
    while (true) {
        do i += 1; while(scores[i] > pivot);
        do j -= 1; while(scores[j] < pivot);

        if (i >= j) { 
            pi = j;
            break;
        }

        SWAP(scores[j], scores[i]);
        SWAP(values[j], values[i]);
    }

    ASSERT(pi >= 0);

    quicksort(scores, values, l, pi);
    quicksort(scores, values, pi+1, r);
}


void sort(Array<fzy_score_t> scores, Array<String> values)
{
    ASSERT(scores.count == values.count);
    quicksort(scores, values, 0, values.count-1);
}


    