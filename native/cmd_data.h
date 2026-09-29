/* gen_cmd.py が生成（ctab.py）。手で編集しない */
#ifndef CMD_DATA_H
#define CMD_DATA_H
typedef struct { unsigned int kn; unsigned short knl; unsigned int dp; unsigned short dpl; unsigned int jo; unsigned short jl; unsigned int fo; unsigned short fl; unsigned int wo; unsigned short wl; unsigned char kind; unsigned char vehicle; unsigned char is_all; unsigned short ooff; unsigned short on_; short vidx; } CmLex;
typedef struct { unsigned int ko; unsigned short kl; unsigned int jo; unsigned short jl; unsigned short mask; unsigned char bare_ok; unsigned char bare_ok2; } CmVerb;
typedef struct { unsigned int po; unsigned short pl; unsigned int dp; unsigned short dpl; unsigned char role; } CmPart;
typedef struct { unsigned int ko; unsigned short kl; unsigned int vo; unsigned short vl; } CmMap;
typedef struct { unsigned int off; unsigned short len; } CmStr;
enum { CMK_VERB, CMK_OBJ, CMK_DIR, CMK_NOCMD };
enum { CMR_O, CMR_WITH, CMR_TO, CMR_IN, CMR_ON, CMR_UNDER, CMR_BEHIND, CMR_FROM, CMR_AND, CMR_EXCEPT, CMR_MOD, CMR_NONE };
enum { CMT_IN = 1, CMT_ON = 2, CMT_AT = 4, CMT_TO = 8, CMT_UNDER = 16, CMT_BEHIND = 32, CMT_FROM = 64, CMT_WITH = 128, CMT_DOWN = 256, CMT_OBJ = 512 };
#define CM_ALL_LEX (cm_k[0])
#define VK_WALK (cm_k[1])
#define VK_CLIMB (cm_k[2])
#define VK_DISEMBARK (cm_k[3])
#define VK_ENTER (cm_k[4])
#define VK_EXIT (cm_k[5])
#define VK_LEAVE (cm_k[6])
#define VK_SWIM (cm_k[7])
#define CM_SCHEMA 0x5F835196u   /* パックの節と突き合わせる（ctab.py） */
extern const unsigned short *cm_jpool; extern unsigned cm_jpool_n;
extern const char *cm_apool; extern unsigned cm_apool_n;
extern const short *cm_k; extern unsigned cm_k_n;   /* 作品に依る定数: CM_ALL_LEX / VK_WALK / VK_CLIMB / VK_DISEMBARK / VK_ENTER / VK_EXIT / VK_LEAVE / VK_SWIM */
extern const CmLex *cm_lex; extern unsigned cm_lex_n;
#define CM_LEX_N cm_lex_n
extern const CmVerb *cm_verbs; extern unsigned cm_verbs_n;
#define CM_VERB_N cm_verbs_n
extern const CmPart *cm_parts; extern unsigned cm_parts_n;
#define CM_PART_N cm_parts_n
extern const CmMap *cm_pwords; extern unsigned cm_pwords_n;
#define CM_PW_N cm_pwords_n
extern const CmMap *cm_yesno; extern unsigned cm_yesno_n;
#define CM_YN_N cm_yesno_n
extern const CmMap *cm_guide; extern unsigned cm_guide_n;
#define CM_GUIDE_N cm_guide_n
extern const CmStr *cm_frags; extern unsigned cm_frags_n;
#define CM_FRAG_N cm_frags_n
extern const CmStr *cm_others; extern unsigned cm_others_n;
extern const CmStr *cm_role_ja; extern unsigned cm_role_ja_n;
#define CM_ROLE_N cm_role_ja_n
#endif
