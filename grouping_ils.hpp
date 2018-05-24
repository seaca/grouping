#ifndef GR_H
#define GR_H
#include <vector>
using namespace std;

class CRand {
public:
  // コンストラクタ
  CRand(int seed = 1)
    : generator_(seed), distribution_(0, 65535) {}
  ~CRand(){}

  // get系
  int get() { return distribution_(generator_); }
private:
  std::mt19937 generator_;
  std::uniform_int_distribution<> distribution_;
};

/*** グループの評価値，相性の最小値，相性の総和を扱うための構造体  ***/
typedef struct gv
{
  double eval;
  int minCompatibility;
  int totalCompatibility;
} GroupValue;

typedef struct st
{
  int id;
  string studentNumber;
  int gender;
  int hennyu;
} Student;

// グループの情報
typedef struct gn
{
  int size;                     // グループの人数
  int totalCompatibility;       // 相性の総和
  int minCompatibility;         // 相性の最小値
  int numMinCompatibility;      // 相性が最小のペアの数
  double evaluation;            // 評価値
  int numGirls;                 // 女子の数
  int numBoys;                  // 男子の数
  int numH;                     // 編入生の数
  bool feasible;                // 制約を満たしているとき true
} GI;



// // {0,...,n-1}上のランダムな置換をvectorに入れて返す
// vector<int> random_permutation(int n);

// /*** ファイルからデータを読み込む ***/
// int read_file(const string &filename);

// /******************************************************************/

// /*** グループkに学生iを追加 ***/
// int joinStudent(int k, int i);

// /*** グループkから学生iを削除する
//      学生がいれば1, いないときは -1 を返す ***/
// int removeStudent(int k, int i);

/*** グループk1の学生s1と，グループk2の学生s2を入れ替える ***/
/*** 交換が成功したら1，できなかったら 0 を返す ***/
int swap_students(int k1, int s1, int k2, int s2);

/*** グループk1の学生s1をグループk2へ移動する ***/
/*** 成功したら1，できなかったら 0 を返す ***/
int move_students(int k1, int s1, int k2);

/******************************************************************/



/*** make_initial_group : ランダムにグループを作る ***/
void make_initial_group();

/*** グループkの情報の一覧を表示 ***/
void printGroup(int k);

/*** すべてのグループ一覧をプリント ***/
void printAllGroup();



/*** 相性から評価値を計算する ***/
double calc_evaluation(int s1, int s2);

/***  グループkの評価値 ***/
//GroupValue evaluateGroup(int k);

/***
 * gr[] : 学生の配列,  gsize : 配列の要素数
 のグループの評価値を返す
***/
double calc_group_evaluation(int *gr, int gsize);

// groupInfo[MAX_NGROUP] のグループ情報を更新する
void update_groupInfo(int k);

/*********************************************************************/

// グループk1の学生s1とグループk2の学生s2を交換したときの，
// 入れ替え後のグループk1とk2の評価値の差を返す
// 戻り値が正なら，交換後の評価値が増加する
double delta_by_swap(int k1, int s1, int k2, int s2);

// (k1, s1) -> (k2, s2) -> (k3, s3)を入れ替えたときの
// 評価値の差を返す
// 戻り値が正なら，交換後の評価値が増加する
double delta_by_3swap(int k1, int s1, int k2, int s2, int k3, int s3);

/*** 単純局所探索 ***/
int local_search_swap();

/*** 3swapを使って近傍により良い解があるかどうかを探す
 *** 解は try3swap 個の3近傍を試す
 *** first_find == true のときは，最初に見つかった改善解へ移動する
 *** first_find == false の場合は，最も良い改善解へ移動する
 *** ある場合はその近傍へ移動して1を返す
 *** ない場合は0を返す ***/
int local_search_by_3swap(int try3swap, bool first_find);

/*** 反復局所探索法 ***/
double iterated_local_search(int **best_grouping, int *best_size, CRand &mt, int default_distance, int try3swap, bool first_find);

// 2-swapをランダムにtimes回行う
void random_swap(int times);

#endif
