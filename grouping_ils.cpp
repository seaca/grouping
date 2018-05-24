#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>
#include <algorithm>
#include <tuple>
#include <ctime>
#include "grouping_ils.hpp"
#include <unistd.h>
using namespace std;

// ここをtrueにすると，色々と出力します
bool DEBUG = true;
//bool DEBUG = false;

/*****************************************************
 *
 *    設定可能なパラメーター
 *
 *****************************************************/

// (1) 相性の最大値
const int MAX_COMPATIBILITY = 10;

// (2) 1グループの人数
// 割り切れないときは，1人少ないグループができます
const int STUDENTS_IN_GROUP = 4;

// (3) 女子1人のグループを許すなら true
// ただし３人以下のグループでは女子一人でも許すとする
const bool PermitGirlAlone = false;

// (4) 男子1人のグループを許すなら true
// ただし３人以下のグループでは男子一人でも許すとする
const bool PermitBoyAlone = false;

// (5) 全て女子を許すなら true
const bool PermitAllGirl = false;

// (6) 全て男子を許すなら true
const bool PermitAllBoy = true;

// (7) 編入生がグループに複数いてもいいなら true
const bool PermitTwoHennyu = false;

// (8) 処理時間のデフォルト値．単位は秒
double LIMIT_TIME = 120;

/*** End: 設定可能なパラメーター *********************/

// ペナルティにかかる係数
double factor = 2.0;

// 3交換近傍を探す回数
// これを0にすると3近傍を探さない
int try3swap = 0;

// trueにすると最初に見つけた3近傍の改善解へ移動する
// falseにすると最も良い改善解へ移動する
bool first_find_in_3swap = true;

/*************************************************/
// グローバル変数（設定の必要なし）

// 学生数の最大値 こうすると150人まで
const int MAX_STUDENTS = 150 + 1;
// グループ数はこの値を越えない
const int MAX_NGROUP = 35;
// グループの人数はこの値を越えない
const int MAX_SIZE = 6;

const int DUMMY  = 10000;
const int USELESS = -100000;


// 相性を格納する配列
int COMPATIBILITY[MAX_STUDENTS][MAX_STUDENTS];
// 作成したグループを格納する配列
int GROUPS[MAX_NGROUP][MAX_SIZE];

// グループの数
int numGroups = 0;

// 学生の数
int numStudents = 0;

// グループ内の相性の最小値
int minimumCompatibility;

// 相性が最小値であるペアの数
int numMinCompatibility;

// 局所最適解からの移動距離
int inline default_distance() { return numGroups; }

/*************************************************/

Student STUDENTS[MAX_STUDENTS];

/*************************************************/

// グループの情報を管理
GI groupInfo[MAX_NGROUP];

// 乱数の seed
unsigned int Seed;
/*************************************************/

typedef struct edge
{
  int group;
  int x;
  int y;
} Edge;

typedef struct gs
{
  int group;
  int id;
} GS;


/**  補助的な関数  *******************************************************/

// {0,...,n-1}上のランダムな置換をvectorに入れて返す
vector<int> random_permutation(int n, CRand& mt)
{
  vector<int> rv;
  for (int i = 0; i < n; i++)
  {
    rv.push_back(i);
  }
  for (int i = 0; i < n-1; i++)
  {
    //auto num = mt.get();
    int num = mt.get();
    int j = (num % n-i) + i;
    int tmp = rv[i];
    rv[i] = rv[j];
    rv[j] = tmp;
  }
  return rv;
}

/**  end: 補助的な関数  **************************************************/

/*** ファイルからデータを読み込む ***/
int read_file(const string &filename, Student *STUDENTS, int COMPATIBILITY[MAX_STUDENTS][MAX_STUDENTS])
{
  ifstream ifs(filename.c_str());
  int count_friendship[MAX_COMPATIBILITY + 1];

  if (ifs.fail())
  {
    cerr << "File does not exist.\n";
    exit(1);
  }

  ifs >> numStudents;           // 学生の総数

  for (int i = 0; i < numStudents; i++)
  {
    string sn;  // 学籍番号
    int gn, hn; // 性別, 編入
    ifs >> sn >> gn >> hn;
    Student st;
    st.id = i;
    st.studentNumber = sn;
    st.gender = gn;
    st.hennyu = hn;
    STUDENTS[i] = st;
  }

  // 友好度を格納する
  int p1, p2, v;
  for (int i = 0; i < numStudents; i++) COMPATIBILITY[i][i] = 0;
  while (ifs >> p1 >> p2 >> v)
  {
    if ((p1==0) && (p2==0) && (v==0)) return 0;
    if ((v <= 0) || (v > MAX_COMPATIBILITY))
    {
      cerr << "Irregal value of friendship" << endl;
      exit(1);
    }
    COMPATIBILITY[p1][p2] = COMPATIBILITY[p2][p1] = v;
    count_friendship[v]++;
  }
  return 1;
}
/*** End: read_file ファイルからデータを読み込む ***/

/*** グループkに学生iを追加 ***/
int joinStudent(int GROUPS[][MAX_SIZE], int k, int i)
{
  int j = 0;
  while (GROUPS[k][j] < numStudents)
  {
    j++;
  }
  GROUPS[k][j] = i;
  groupInfo[k].size++;

  return k;
}

/*** グループkから学生iを削除する
     学生がいれば1, いないときは 0 を返す
 ***/
int removeStudent(int k, int i)
{
  int j;
  bool found = false;

  if (groupInfo[k].size == 0) return 0;

  for (j = 0; j < groupInfo[k].size; j++)
  {
    if (i == GROUPS[k][j])
    {
      found = true;
      break;
    }
  }

  if (found)
  {
    GROUPS[k][j] = DUMMY;
    groupInfo[k].size--;
    sort(GROUPS[k], GROUPS[k] + MAX_SIZE);
    return 1;
  }
  else
    return 0;
}





/*** グループk1の学生s1と，グループk2の学生s2を入れ替える ***/
/*** 交換が成功したら1，できなかったら 0 を返す ***/
int swap_students(int k1, int s1, int k2, int s2)
{
  if (k1 == k2) return 0;

  int s1_sucess = removeStudent(k1, s1);
  int s2_sucess = removeStudent(k2, s2);

  if (!s1_sucess && !s2_sucess) return 0;
  else if (!s1_sucess && s2_sucess)
  {
    joinStudent(GROUPS, k2, s2);
    return 0;
  }
  else if (s1_sucess && !s2_sucess)
  {
    joinStudent(GROUPS, k1, s1);
    return 0;
  }
  else
  {
    joinStudent(GROUPS, k1, s2);
    joinStudent(GROUPS, k2, s1);
    return 1;
  }
}

/*** グループk1の学生s1をグループk2へ移動する ***/
/*** 成功したら1，できなかったら 0 を返す ***/
int move_students(int k1, int s1, int k2)
{
  if (k1 == k2) return 0;

  int s1_sucess = removeStudent(k1, s1);

  if (!s1_sucess) return 0;
  else
  {
    joinStudent(GROUPS, k2, s1);
    return 1;
  }
}

/*** make_initial_group : ランダムにグループを作る ***/
void make_initial_group(CRand &mt)
{
  // グループ数
  numGroups = numStudents / STUDENTS_IN_GROUP;
  if (numStudents % STUDENTS_IN_GROUP > 0) numGroups++;

  // initialize
  for (int k=0; k<numGroups; k++)
  {
    for (int i=0; i<MAX_SIZE; i++)
    {
      GROUPS[k][i] = DUMMY;
    }
  }

  // グループに入る人数を計算しておく
  for (int k=0; k<numGroups; k++)
  {
    groupInfo[k].size = 0;
  }

  vector<int> rv = random_permutation(numStudents, mt);
  int s = 0;
  int k = 0;
  for (s=0; s < numStudents; s++)
  {
    joinStudent(GROUPS, k, rv[s]);
    if (k == numGroups - 1) k = 0;
    else k++;
  }
}  // End: make_initial_group


/*** グループkの一覧を表示 ***/
void printGroup(int k)
{
  int* vs = GROUPS[k];
  sort(vs, vs + groupInfo[k].size);
  for (int i = 0; i < groupInfo[k].size; i++)
  {
    int st = vs[i];
    printf("%3d", vs[i]);
    if (STUDENTS[st].gender && STUDENTS[st].hennyu) printf("gH");
    else if (STUDENTS[st].gender && !STUDENTS[st].hennyu) printf("g ");
    else if (!STUDENTS[st].gender && STUDENTS[st].hennyu) printf("H ");
    else printf("  ");
  }
  if (groupInfo[k].size < STUDENTS_IN_GROUP)
  {
    printf("     |");
  }
  else
  {
    printf("|");
  }
  // グループ内の相性
  vector<int> edges;
  for (int i = 0; i < groupInfo[k].size; i++)
  {
    for (int j = i + 1; j < groupInfo[k].size; j++)
    {
      int s1 = GROUPS[k][i];
      int s2 = GROUPS[k][j];
      int r = COMPATIBILITY[s1][s2];
      edges.push_back(r);
    }
  }
  for (int i=0; i < (int)edges.size(); i++)
  {
    printf("%3d", edges[i]);
  }

  GI *gv = &groupInfo[k];
  printf("  %5.1f %3d", gv->evaluation, gv->totalCompatibility);
  printf("  %d %d %d", gv->numGirls, gv->numBoys, gv->numH);

  if (!gv->feasible) printf(" NG\n");
  else printf("\n");
}
/*** End : PrintGroup ***/

/*** グループkの一覧を表示 ***/
void printGroupCsv(int k)
{
  int* vs = GROUPS[k];
  sort(vs, vs + groupInfo[k].size);
  // グループの番号と人数
  printf("%d,%d,", k,groupInfo[k].size);
  // メンバー一覧
  for (int i = 0; i < groupInfo[k].size; i++)
  {
    int st = vs[i];
    printf("%d", vs[i]);
    if (STUDENTS[st].gender && STUDENTS[st].hennyu) printf("gH,");
    else if (STUDENTS[st].gender && !STUDENTS[st].hennyu) printf("g,");
    else if (!STUDENTS[st].gender && STUDENTS[st].hennyu) printf("H,");
    else printf(",");
  }
  // 人数が一人少ないグループでは一つ空白を入れる
  if (groupInfo[k].size < STUDENTS_IN_GROUP)
  {
    printf(" ,");
  }
  // グループ内の相性の一覧
  vector<int> edges;
  for (int i = 0; i < groupInfo[k].size; i++)
  {
    for (int j = i + 1; j < groupInfo[k].size; j++)
    {
      int s1 = GROUPS[k][i];
      int s2 = GROUPS[k][j];
      int r = COMPATIBILITY[s1][s2];
      edges.push_back(r);
    }
  }
  for (int i=0; i < (int)edges.size(); i++)
  {
    printf("%d ", edges[i]);
  }
  printf(",");

  GI *gv = &groupInfo[k];
  printf("%.1f,%d,", gv->evaluation, gv->totalCompatibility);
  printf("%d,%d,%d,", gv->numGirls, gv->numBoys, gv->numH);

  // ルールに反するグループではNGと表示
  if (!gv->feasible) printf("NG\n");
  else printf("\n");
}
/*** End : PrintGroupCsv ***/

/*** グループ一覧をプリント ***/
void printAllGroup()
{
  double tv = 0.0;
  int tc = 0;
  for (int k = 0; k < numGroups; k++)
  {
    //printGroup(k);
    printGroupCsv(k);
    tv += groupInfo[k].evaluation;
    tc += groupInfo[k].totalCompatibility;
  }

  printf("Total Value = %3.1f\n", tv);
  printf("Minimum Compatibility = %d\n", minimumCompatibility);
  printf("Num of Minimum Compatibility = %d\n", numMinCompatibility);
  printf("Total Compatibility = %d\n", tc);
}
/*** End : PrintAllgroup グループをプリント ***/

/***
 * gr[] : 学生の配列,  gsize : 配列の要素数
 のグループの評価値を返す
***/
double calc_group_evaluation(int *gr, int gsize)
{
  double val = 0.0;             // グループの評価値

  for (int i = 0; i < gsize; i++)
  {
    for (int j = i + 1; j < gsize; j++)
    {
      int s1 = gr[i];
      int s2 = gr[j];
      val += calc_evaluation(s1, s2);
    }
  }

  // 制約に違反していたら，ペナルティとしてグループの評価値を下げる
  int ng = 0;
  int nb = 0;
  int nh = 0;
  for (int i = 0; i < gsize; i++)
  {
    Student s = STUDENTS[gr[i]];
    if (s.gender) ng++; else nb++;
    if (s.hennyu) nh++;
  }
  // 制約に反した分だけPenaltyを増加
  double penalty = 0;
  if (!PermitGirlAlone && (ng == 1)) penalty++;
  if (!PermitBoyAlone && (nb == 1)) penalty++;
  if (!PermitAllGirl && (nb == 0)) penalty++;
  if (!PermitAllBoy && (ng == 0)) penalty++;
  if (!PermitTwoHennyu && (nh > 1)) penalty++;

  // 評価値を penalty の分だけ減少する
  double eval = val / (1 + penalty);  // <-- 評価値の減少
  return eval;
}
/*********************************************************************/

// groupInfo[MAX_NGROUP] のグループ情報を更新する
void update_groupInfo(int k)
{
  double val = 0.0;             // グループの評価値
  int mc = MAX_COMPATIBILITY;   // 相性の最小値
  int num_mc = 0;               // 相性が最小のペアの数
  int tc = 0;                   // 相性の総和
  int sz = 0;                   // 人数

  for (int i = 0; i < MAX_SIZE; i++)
    if (GROUPS[k][i] < MAX_STUDENTS) sz++;

  for (int i = 0; i < sz; i++)
  {
    for (int j = i + 1; j < sz; j++)
    {
      int s1 = GROUPS[k][i];
      int s2 = GROUPS[k][j];
      int f = COMPATIBILITY[s1][s2];
      val += calc_evaluation(s1, s2);
      if (f < mc)
      {
        mc = f;
      }
      tc += f;
    }
  }

  for (int i = 0; i < sz; i++)
    for (int j = i + 1; j < sz; j++)
    {
      int s1 = GROUPS[k][i];
      int s2 = GROUPS[k][j];
      int f = COMPATIBILITY[s1][s2];

      if (f == mc) num_mc++;
    }

  groupInfo[k].minCompatibility = mc;
  groupInfo[k].numMinCompatibility = num_mc;

  for (int i = 0; i < sz; i++)
    for (int j = i + 1; j < sz; j++)
    {
      int s1 = GROUPS[k][i];
      int s2 = GROUPS[k][j];

      if (s1 == s2)
      {
        printf("Student %d is doubled in Group %d\n", s1, k);
        printf("Seed = %u\n", Seed);
        exit(1);
      }
    }

  // 学生のチェック
  int g = 0, b = 0, h = 0;
  for (int i = 0; i < sz; i++)
  {
    int s = GROUPS[k][i];
    Student st = STUDENTS[s];

    if (st.gender) g++;  else b++;
    if (st.hennyu) h++;
  }
  groupInfo[k].numGirls = g;
  groupInfo[k].numBoys = b;
  groupInfo[k].numH = h;

  // 制約に違反していたら，ペナルティとしてグループの評価値を下げる
  // 制約に反した分だけPenaltyを増加
  double penalty = 0;
  bool feasible = true;
  if (!PermitGirlAlone && (groupInfo[k].numGirls == 1) && (groupInfo[k].size >= 4)) { penalty++; feasible = false; }
  if (!PermitBoyAlone && (groupInfo[k].numBoys == 1) && (groupInfo[k].size >= 4)) { penalty++; feasible = false; }
  if (!PermitAllGirl && (groupInfo[k].numBoys == 0)) { penalty++; feasible = false; }
  if (!PermitAllBoy && (groupInfo[k].numGirls == 0)) { penalty++; feasible = false; }
  if (!PermitTwoHennyu && (groupInfo[k].numH > 1)) { penalty++; feasible = false; }

  // 評価値を penalty の分だけ減少する
  groupInfo[k].size = sz;
  groupInfo[k].evaluation = val / (1 + penalty);  // <-- 評価値の減少
  groupInfo[k].totalCompatibility = tc;
  groupInfo[k].numGirls = g;
  groupInfo[k].numBoys = b;
  groupInfo[k].numH = h;
  groupInfo[k].feasible = feasible;
}
// End: update_groupInfo

// グループk1の学生s1とグループk2の学生s2を交換したときの，
// 入れ替え後のグループk1とk2の評価値の差を返す
// 戻り値が正なら，交換後の評価値が増加する
double delta_by_swap(int k1, int s1, int k2, int s2)
{
  // 交換前のグループの評価値
  double before = groupInfo[k1].evaluation + groupInfo[k2].evaluation;

  // 交換後のグループを作る
  int group_k1[MAX_SIZE];
  int group_k2[MAX_SIZE];

  int j = 0;
  for (int i=0; i < groupInfo[k1].size; i++)
  {
    int s = GROUPS[k1][i];
    if (s != s1)
    {
      group_k1[j] = s;
      j++;
    }
  }
  group_k1[groupInfo[k1].size - 1] = s2;

  j = 0;
  for (int i=0; i < groupInfo[k2].size; i++)
  {
    int s = GROUPS[k2][i];
    if (s != s2)
    {
      group_k2[j] = s;
      j++;
    }
  }
  group_k2[groupInfo[k2].size - 1] = s1;

  // 交換後の評価値
  double after = calc_group_evaluation(group_k1, groupInfo[k1].size) +
    calc_group_evaluation(group_k2, groupInfo[k2].size);
  // 評価値の差を返す
  return after - before;
} // end delta_by_swap


// (k1, s1) -> (k2, s2) -> (k3, s3)を入れ替えたときの
// 評価値の差を返す
// 戻り値が正なら，交換後の評価値が増加する
double delta_by_3swap(int k1, int s1, int k2, int s2, int k3, int s3)
{
  // 交換前のグループの評価値
  double before = groupInfo[k1].evaluation + groupInfo[k2].evaluation + groupInfo[k3].evaluation;
  // 交換後のグループを作る
  int group_k1[STUDENTS_IN_GROUP];
  int group_k2[STUDENTS_IN_GROUP];
  int group_k3[STUDENTS_IN_GROUP];

  // s3 -> k1
  int j = 0;
  for (int i=0; i < groupInfo[k1].size; i++)
  {
    int s = GROUPS[k1][i];
    if (s != s1)
    {
      group_k1[j] = s;
      j++;
    }
  }
  group_k1[groupInfo[k1].size - 1] = s3;

  // s1 -> k2
  j = 0;
  for (int i=0; i < groupInfo[k2].size; i++)
  {
    int s = GROUPS[k2][i];
    if (s != s2)
    {
      group_k2[j] = s;
      j++;
    }
  }
  group_k2[groupInfo[k2].size - 1] = s1;

  // s2 -> k3
  j = 0;
  for (int i=0; i < groupInfo[k3].size; i++)
  {
    int s = GROUPS[k3][i];
    if (s != s3)
    {
      group_k3[j] = s;
      j++;
    }
  }
  group_k3[groupInfo[k3].size - 1] = s2;

  // 交換後の評価値
  double after = calc_group_evaluation(group_k1, groupInfo[k1].size)
    + calc_group_evaluation(group_k2, groupInfo[k2].size)
    + calc_group_evaluation(group_k3, groupInfo[k3].size);

  // 評価値の差を返す
  return after - before;
} // end calc_eval_by_3swap


/*** 単純局所探索 ***/
int local_search_swap(CRand &mt)
{
  // 局所最適解が見つかったらtrue
  bool found_local_opt = false;
  do
  {
    vector<pair<GS,GS> > exchange_pair;
    double max_difference = 0;

    int k1, k2;
    int p1, p2;
    for (k1 = 0; k1 < numGroups; k1++)
    {
      for (k2 = k1+1; k2 < numGroups; k2++)
      {
        for (p1 = 0; p1 < groupInfo[k1].size; p1++)
        {
          for (p2 = 0; p2 < groupInfo[k2].size; p2++)
          {
            int s1 = GROUPS[k1][p1];
            int s2 = GROUPS[k2][p2];

            // 二つのグループの評価値
            double delta = delta_by_swap(k1, s1, k2, s2);
            if (delta > 0)
            {
              GS gs1, gs2;
              if (delta >= max_difference)
              {
                if (delta > max_difference)
                {
                  max_difference = delta;
                  exchange_pair.clear();
                }
                gs1.group = k1;
                gs1.id = s1;
                gs2.group = k2;
                gs2.id = s2;
                exchange_pair.push_back(make_pair(gs1, gs2));
              }
            }
          }
        }
      }
    } // 全ての近傍をチェックした

    // exchange_pairには，1番評価値を上げるペアの組が入っている
    if (exchange_pair.empty())
    {
      found_local_opt = true;
      if (DEBUG)
      {
        printf("------------\n");
        printf("       Found Local Optima by 2-swap.\n");
      }
    }
    else
    {
      int p = mt.get() % exchange_pair.size();
      GS gs1 = exchange_pair[p].first;
      GS gs2 = exchange_pair[p].second;
      int k1 = gs1.group, s1 = gs1.id;
      int k2 = gs2.group, s2 = gs2.id;

      if (DEBUG)
      {
        printf("       Swap (%d,%d) <--> (%d,%d)\n", k1, s1, k2, s2);
      }
      swap_students(k1, s1, k2, s2);
      update_groupInfo(k1);
      update_groupInfo(k2);

      //if (DEBUG) printAllGroup();
    }
  } while (!found_local_opt);
  return 1;
} /* End: local_search */


/*** 3swapを使って近傍により良い解があるかどうかを探す
 *** 解は try3swap 個の3近傍を試す
 *** first_find == true のときは，最初に見つかった改善解へ移動する
 *** first_find == false の場合は，最も良い改善解へ移動する
 *** ある場合はその近傍へ移動して1を返す
 *** ない場合は0を返す ***/
int local_search_by_3swap(int try3swap, bool first_find, CRand &mt)
{
  if (try3swap == 0) return 0;
  if (DEBUG) printf("          Try 3-swap\n");
  double max_difference = 0.0;
  int k1, k2, k3;
  int p1, p2, p3;
  vector<tuple<GS, GS, GS> > exchange_triple;

  // 3-Swapの回数
  int IterationTimeof3Swap = 0;

  while (IterationTimeof3Swap < try3swap)
  {
    IterationTimeof3Swap++;

    // ランダムに3個のグループを選択
    k1 = mt.get() % numGroups;
    do {
      k2 = mt.get() % numGroups;
    } while (k1 == k2);
    do {
      k3 = mt.get() % numGroups;
    } while ((k3 == k1) || (k3 == k2));

    // 3個のグループk1, k2, k3を選んだ
    for (p1 = 0; p1 < groupInfo[k1].size; p1++)
    {
      for (p2 = 0; p2 < groupInfo[k2].size; p2++)
      {
        for (p3 = 0; p3 < groupInfo[k3].size; p3++)
        {
          int s1 = GROUPS[k1][p1];
          int s2 = GROUPS[k2][p2];
          int s3 = GROUPS[k3][p3];

          // 入れ替え後の評価値の差の計算
          double delta1 = delta_by_3swap(k1, s1, k2, s2, k3, s3);
          double delta2 = delta_by_3swap(k3, s3, k2, s2, k1, s1);
          double delta = max(delta1, delta2);

          if (first_find)
          {
            // first_find == true のとき
            if (delta > 0)
            {
              if (delta1 > delta2)
              {
                // s1 -> s2 -> s3
                swap_students(k1, s1, k2, s2);
                swap_students(k2, s2, k3, s3);
              }
              else
              {
                // s1 -> s3 -> s2
                swap_students(k1, s1, k2, s2);
                swap_students(k1, s1, k3, s3);
              }
              // 改善解を見つけたので終了
              return 1;
            }
          }
          else
          {
            if (delta > 0)
            {
              if (delta >= max_difference)
              {
                if (delta > max_difference)
                {
                  max_difference = delta;
                  exchange_triple.empty();
                }
                GS gs1, gs2, gs3;
                gs1.group = k1;
                gs1.id = s1;
                gs2.group = k2;
                gs2.id = s2;
                gs3.group = k3;
                gs3.id = s3;

                if (delta1 > delta2)
                {
                  exchange_triple.push_back(make_tuple(gs1, gs2, gs3));
                }
                else
                {
                  exchange_triple.push_back(make_tuple(gs3, gs2, gs1));
                }
              }
            }
          }
        }
      }
    }
  } // 3-Swapによる探索終了

  // first_findが0のときは，ここで終了（改善解がなかった）
  if (first_find) return 0;

  // exchange_tupleには，1番評価値を上げる3-tupleの組が入っている
  if (!exchange_triple.empty())
  {
    // ランダムに一つ選んで交換する
    int p = mt.get() % exchange_triple.size();
    GS gs1 = get<0>(exchange_triple[p]);
    GS gs2 = get<1>(exchange_triple[p]);
    GS gs3 = get<2>(exchange_triple[p]);
    int k1 = gs1.group, s1 = gs1.id;
    int k2 = gs2.group, s2 = gs2.id;
    int k3 = gs3.group, s3 = gs3.id;

    if (DEBUG)
    {
      printf("------------\n");
      printf("       3-Swap (%d,%d) -> (%d,%d) -> (%d,%d)\n", k1, s1, k2, s2, k3, s3);
    }

    swap_students(k1, s1, k2, s2);
    swap_students(k2, s2, k3, s3);

    return 1;
  }
  else
  {
    // 改善解は見つからなかった
    if (DEBUG)
    {
      printf("------------\n");
      printf("          Found Local Optima by 3-swap\n");
    }
    return 0;
  } // 3-Swap終了
} // End: local_search_by_3swap

/*********************************************************************/
/*********************************************************************/
/*********************************************************************/
/*********************************************************************/

/***     ここまでは共通     ******************************************/

/*********************************************************************/

/*** s1とs2の相性から評価値を計算する ***/
double calc_evaluation(int s1, int s2)
{
  if (s1 == s2) return 0;
  int f = COMPATIBILITY[s1][s2];
  int d = max(0, minimumCompatibility - f + 1);

  f = f - factor * d;

  return f;
}
/*********************************************************************/


// 2-swapをランダムにtimes回行う
void random_swap(int times, CRand &mt)
{
  if (DEBUG) printf(" random_swap\n");

  for (int p = 0; p < times; p++)
  {
    // ランダムに2個のグループを選択
    int k1 = mt.get() % numGroups, k2;
    do {
      k2 = mt.get() % numGroups;
    } while (k1 == k2);

    int p1 = mt.get() % groupInfo[k1].size;
    int p2 = mt.get() % groupInfo[k2].size;
    int s1 = GROUPS[k1][p1];
    int s2 = GROUPS[k2][p2];

    int SWAP_SUCESS = swap_students(k1, s1, k2, s2);
    if (!SWAP_SUCESS)
    {
      printf("Error\n");
      exit(1);
    }
  }
}

/*** 反復局所探索法 ***/
double iterated_local_search(int best_grouping[][MAX_SIZE], int best_size[], CRand &mt, int try3swap, bool first_find)
{
  int maximumTotalCompatibility = 0;
  double maximumTotalValue = 0.0;
  double previous_tc = -100.0;

  clock_t iteration_start = clock();       // スタート時間
  clock_t iteration_end = iteration_start; // 終了時間

  int iter = 0;
  bool initialize = true;
  // 最良解が更新されなかった回数
  int not_improved = 0;
  /*** 終了条件 ***/
  // 時間が LIMIT_TIME秒 を超えたら終了する
  // 改善解が見つからないループが MAX_NOTIMPROVE 回繰り返されたら終了する
  //  while ((iter < MIN_ITERATION)
  //         || ((double)(iteration_end - iteration_start) / CLOCKS_PER_SEC < LIMIT_TIME
  //             && not_improved < MAX_NOT_IMPROVED)
  while ((double)(iteration_end - iteration_start) / CLOCKS_PER_SEC < LIMIT_TIME)
  {
    if (initialize)
    {
      make_initial_group(mt);
      initialize = false;

      for (int k = 0; k < numGroups; k++) update_groupInfo(k);

      minimumCompatibility = MAX_COMPATIBILITY;
      numMinCompatibility = 0;

      for (int k = 0; k < numGroups; k++)
        if (groupInfo[k].minCompatibility < minimumCompatibility)
        {
          minimumCompatibility = groupInfo[k].minCompatibility;
        }
      for (int k = 0; k < numGroups; k++)
      {
        if (groupInfo[k].minCompatibility == minimumCompatibility)
        {
          numMinCompatibility += groupInfo[k].numMinCompatibility;
        }
      }
      initialize = false;

      if (DEBUG)
      {
        printf("Initial Grouping\n");
        for (int k = 0; k < numGroups; k++) update_groupInfo(k);
        printAllGroup();
      }
    }
    // グループ初期生成と minimumCompatibility と numMinCompatibility の計算終了


    // ここから局所探索
    for (int k = 0; k < numGroups; k++) update_groupInfo(k);

    int not_local_opt = 1;
    do
    {
      // 局所最適解へ移動
      local_search_swap(mt);
      // 3-swapを試す
      not_local_opt = local_search_by_3swap(try3swap, first_find_in_3swap, mt);
      if (DEBUG)
      {
        if (not_local_opt) printf("Found better 3-neighbor\n");
        for (int k = 0; k < numGroups; k++) update_groupInfo(k);
        printAllGroup();
      }
    } while (not_local_opt);

    // 現在の解をチェック
    int current_mc = MAX_COMPATIBILITY;
    int current_tc = 0;
    double current_tv = 0.0;
    bool feasible = true;
    for (int k = 0; k < numGroups; k++)
    {
      GI *gi = &groupInfo[k];
      current_mc = min(current_mc, gi->minCompatibility);
      current_tc += gi->totalCompatibility;
      current_tv += gi->evaluation;

      if (!PermitGirlAlone && (groupInfo[k].numGirls == 1)) { feasible = false; break; }
      if (!PermitBoyAlone && (groupInfo[k].numBoys == 1)) { feasible = false; break; }
      if (!PermitAllGirl && (groupInfo[k].numBoys == 0)) { feasible = false; break; }
      if (!PermitAllBoy && (groupInfo[k].numGirls == 0)) { feasible = false; break; }
      if (!PermitTwoHennyu && (groupInfo[k].numH > 1)) { feasible = false; break; }
    }

    iter++;

    // 実行可能解である かつ
    // 相性の最小値が上回る，または，相性の最小値が同じかつ相性の総和が大きい
    // ならば，新しい解は良い解
    if (feasible &&
        ((minimumCompatibility < current_mc)
        ||
        ((minimumCompatibility == current_mc) && (maximumTotalCompatibility < current_tc)))
        )
    {
      minimumCompatibility = current_mc;
      maximumTotalCompatibility = current_tc;
      maximumTotalValue = current_tv;

      for (int k=0; k<numGroups; k++)
      {
        for (int i=0; i<groupInfo[k].size; i++)
        {
          best_grouping[k][i] = GROUPS[k][i];
        }
        best_size[k] = groupInfo[k].size;
      }
      not_improved = 0;

      if (DEBUG) printAllGroup();
    }
    else
    {
      if (DEBUG)
      {
        if (!feasible)
          printf("       Infeasible Solution: %d\n", not_improved);
        else
          printf("       Not Improved: %d\n", not_improved);
      }
      not_improved++;
    }

    // 現在の解から距離 step の近傍へ移動する
    // 直前の局所解に戻ってきたときは，より遠い解へ飛ばす
    int distance = default_distance();
    if (current_tc == previous_tc)
    {
      distance += distance;
    }
    else
    {
      distance = default_distance();
      previous_tc = current_tc;
    }

    random_swap(distance, mt);

    // 終了時間
    iteration_end = clock();
  }
  return maximumTotalValue;
}


/*** 反復局所探索法 old ***/
double iterated_local_search0(int best_grouping[][MAX_SIZE], int best_size[], CRand &mt, int try3swap, bool first_find)
{
  make_initial_group(mt);

  for (int k = 0; k < numGroups; k++) update_groupInfo(k);

  minimumCompatibility = MAX_COMPATIBILITY;
  numMinCompatibility = 0;

  for (int k = 0; k < numGroups; k++)
    if (groupInfo[k].minCompatibility < minimumCompatibility)
    {
      minimumCompatibility = groupInfo[k].minCompatibility;
    }
  for (int k = 0; k < numGroups; k++)
  {
    if (groupInfo[k].minCompatibility == minimumCompatibility)
    {
      numMinCompatibility += groupInfo[k].numMinCompatibility;
    }
  }
  // minimumCompatibility と numMinCompatibility の計算終了

  if (DEBUG)
  {
    printf("Initial Grouping\n");
    for (int k = 0; k < numGroups; k++) update_groupInfo(k);
    printAllGroup();
  }

  int maximumTotalCompatibility = 0;
  double maximumTotalValue = 0.0;
  double previous_tc = -100.0;

  clock_t iteration_start = clock();       // スタート時間
  clock_t iteration_end = iteration_start; // 終了時間

  int iter = 0;
  // 最良解が更新されなかった回数
  int not_improved = 0;
  /*** 終了条件 ***/
  // 時間が LIMIT_TIME秒 を超えたら終了する
  // 改善解が見つからないループが MAX_NOTIMPROVE 回繰り返されたら終了する
  //  while ((iter < MIN_ITERATION)
  //         || ((double)(iteration_end - iteration_start) / CLOCKS_PER_SEC < LIMIT_TIME
  //             && not_improved < MAX_NOT_IMPROVED)
  while ((double)(iteration_end - iteration_start) / CLOCKS_PER_SEC < LIMIT_TIME)
  {
    for (int k = 0; k < numGroups; k++) update_groupInfo(k);

    int not_local_opt = 1;
    do
    {
      // 局所最適解へ移動
      local_search_swap(mt);
      // 3-swapを試す
      not_local_opt = local_search_by_3swap(try3swap, first_find_in_3swap, mt);
      if (DEBUG)
      {
        if (not_local_opt) printf("Found better 3-neighbor\n");
        for (int k = 0; k < numGroups; k++) update_groupInfo(k);
        printAllGroup();
      }
    } while (not_local_opt);

    // 現在の解をチェック
    int current_mc = MAX_COMPATIBILITY;
    int current_tc = 0;
    double current_tv = 0.0;
    bool feasible = true;
    for (int k = 0; k < numGroups; k++)
    {
      GI *gi = &groupInfo[k];
      current_mc = min(current_mc, gi->minCompatibility);
      current_tc += gi->totalCompatibility;
      current_tv += gi->evaluation;

      if (!PermitGirlAlone && (groupInfo[k].numGirls == 1)) { feasible = false; break; }
      if (!PermitBoyAlone && (groupInfo[k].numBoys == 1)) { feasible = false; break; }
      if (!PermitAllGirl && (groupInfo[k].numBoys == 0)) { feasible = false; break; }
      if (!PermitAllBoy && (groupInfo[k].numGirls == 0)) { feasible = false; break; }
      if (!PermitTwoHennyu && (groupInfo[k].numH > 1)) { feasible = false; break; }
    }

    iter++;

    // 実行可能解である かつ
    // 相性の最小値が上回る，または，相性の最小値が同じかつ相性の総和が大きい
    // ならば，新しい解は良い解
    if (feasible &&
        ((minimumCompatibility < current_mc)
        ||
        ((minimumCompatibility == current_mc) && (maximumTotalCompatibility < current_tc)))
        )
    {
      minimumCompatibility = current_mc;
      maximumTotalCompatibility = current_tc;
      maximumTotalValue = current_tv;

      for (int k=0; k<numGroups; k++)
      {
        for (int i=0; i<groupInfo[k].size; i++)
        {
          best_grouping[k][i] = GROUPS[k][i];
        }
        best_size[k] = groupInfo[k].size;
      }
      not_improved = 0;

      if (DEBUG) printAllGroup();
    }
    else
    {
      if (DEBUG)
      {
        if (!feasible)
          printf("       Infeasible Solution: %d\n", not_improved);
        else
          printf("       Not Improved: %d\n", not_improved);
      }
      not_improved++;
    }

    // 現在の解から距離 step の近傍へ移動する
    // 直前の局所解に戻ってきたときは，より遠い解へ飛ばす
    int distance = default_distance();
    if (current_tc == previous_tc)
    {
      distance += distance;
    }
    else
    {
      distance = default_distance();
      previous_tc = current_tc;
    }

    random_swap(distance, mt);

    // 終了時間
    iteration_end = clock();
  }
  return maximumTotalValue;
}

int main(int argc, char** argv)
{
  string filename;
  int best_grouping[MAX_NGROUP][MAX_SIZE];
  int best_size[MAX_NGROUP];

  random_device rd;
  //CRand mt(34); // デバック用
  Seed = rd();
  CRand mt(Seed);

  if (DEBUG) printf("Seed = %u\n", Seed);

  int opt;
  opterr = 0;

  while ((opt = getopt(argc, argv, "t:")) != -1)
  {
    //コマンドライン引数のオプションがなくなるまで繰り返す
    switch (opt) {
    case 't':
      LIMIT_TIME = atof(optarg);
      break;

    default:
      //指定していないオプションが渡された場合
      printf("Usage: %s [-t time] filename\n", argv[0]);
      break;
    }
  }
  if (argc <= 1)
  {
    cerr << "Please give a problem filename." << endl;
    return 1;
  }
  else
  {
    filename = string(argv[optind]);
  }

  read_file(filename, STUDENTS, COMPATIBILITY);

  cout << filename << endl;
  iterated_local_search(best_grouping, best_size, mt, try3swap, first_find_in_3swap);

  for (int k=0; k<numGroups; k++)
  {
    for (int i=0; i<groupInfo[k].size; i++)
    {
      GROUPS[k][i] = best_grouping[k][i];
    }
    groupInfo[k].size = best_size[k];
  }
  for (int k=0; k<numGroups; k++) update_groupInfo(k);
  printAllGroup();

  return 0;
}
