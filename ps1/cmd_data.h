/* gen_cmd.py が生成。手で編集しない */
#ifndef CMD_DATA_H
#define CMD_DATA_H
typedef struct { unsigned int kn; unsigned short knl; unsigned int dp; unsigned short dpl;
  unsigned int jo; unsigned short jl; unsigned int wo; unsigned short wl;
  unsigned char kind, vehicle, is_all; unsigned short ooff, on_; short vidx; } CmLex;
typedef struct { unsigned int ko; unsigned short kl; unsigned int jo; unsigned short jl;
  unsigned short mask; unsigned char bare_ok, bare_ok2; } CmVerb;
typedef struct { unsigned int po; unsigned short pl; unsigned int dp; unsigned short dpl; unsigned char role; } CmPart;
typedef struct { unsigned int ko; unsigned short kl; unsigned int vo; unsigned short vl; } CmMap;
typedef struct { unsigned int off; unsigned short len; } CmStr;
enum { CMK_VERB, CMK_OBJ, CMK_DIR };
enum { CMR_O, CMR_WITH, CMR_TO, CMR_IN, CMR_ON, CMR_UNDER, CMR_BEHIND, CMR_FROM, CMR_AND, CMR_EXCEPT, CMR_MOD, CMR_NONE };
enum { CMT_IN = 1, CMT_ON = 2, CMT_AT = 4, CMT_TO = 8, CMT_UNDER = 16, CMT_BEHIND = 32, CMT_FROM = 64, CMT_WITH = 128, CMT_DOWN = 256, CMT_OBJ = 512 };
enum { CM_ALL_LEX = 654 };
enum { CM_LEX_N = 1092, CM_VERB_N = 126, CM_PART_N = 30, CM_PW_N = 7, CM_YN_N = 2, CM_GUIDE_N = 8, CM_FRAG_N = 19, CM_ROLE_N = 11 };
enum { VK_WALK = 118 };
enum { VK_CLIMB = 12 };
enum { VK_DISEMBARK = 23 };
enum { VK_ENTER = 30 };
enum { VK_EXIT = 32 };
enum { VK_LEAVE = 52 };
enum { VK_SWIM = 103 };
extern const unsigned short cm_jpool[];
extern const char cm_apool[];
extern const CmLex cm_lex[CM_LEX_N];
extern const CmVerb cm_verbs[CM_VERB_N];
extern const CmPart cm_parts[CM_PART_N];
extern const CmMap cm_pwords[CM_PW_N];
extern const CmMap cm_yesno[CM_YN_N];
extern const CmMap cm_guide[CM_GUIDE_N];
extern const CmStr cm_frags[CM_FRAG_N];
extern const CmStr cm_others[];
extern const CmStr cm_role_ja[CM_ROLE_N];
#endif
