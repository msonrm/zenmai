/* gen_cmd.py が生成（ctab.py）。手で編集しない。PC-98 版: 表はパックの節から読む（tabload.c） */
#include <stddef.h>
#include "tabload.h"
#include "cmd_data.h"
const unsigned short *cm_jpool; unsigned cm_jpool_n;
const char *cm_apool; unsigned cm_apool_n;
const short *cm_k; unsigned cm_k_n;
const CmLex *cm_lex; unsigned cm_lex_n;
const CmVerb *cm_verbs; unsigned cm_verbs_n;
const CmPart *cm_parts; unsigned cm_parts_n;
const CmMap *cm_pwords; unsigned cm_pwords_n;
const CmMap *cm_yesno; unsigned cm_yesno_n;
const CmMap *cm_guide; unsigned cm_guide_n;
const CmStr *cm_frags; unsigned cm_frags_n;
const CmStr *cm_others; unsigned cm_others_n;
const CmStr *cm_role_ja; unsigned cm_role_ja_n;
const TlTab cmd_tabs[] = {
  {"cm_jpool", (const void **)&cm_jpool, &cm_jpool_n, sizeof(unsigned short), 1, {{0,2}}},
  {"cm_apool", (const void **)&cm_apool, &cm_apool_n, sizeof(char), 1, {{0,1}}},
  {"cm_k", (const void **)&cm_k, &cm_k_n, sizeof(short), 1, {{0,2}}},
  {"cm_lex", (const void **)&cm_lex, &cm_lex_n, sizeof(CmLex), 16, {{offsetof(CmLex,kn),4},{offsetof(CmLex,knl),2},{offsetof(CmLex,dp),4},{offsetof(CmLex,dpl),2},{offsetof(CmLex,jo),4},{offsetof(CmLex,jl),2},{offsetof(CmLex,fo),4},{offsetof(CmLex,fl),2},{offsetof(CmLex,wo),4},{offsetof(CmLex,wl),2},{offsetof(CmLex,kind),1},{offsetof(CmLex,vehicle),1},{offsetof(CmLex,is_all),1},{offsetof(CmLex,ooff),2},{offsetof(CmLex,on_),2},{offsetof(CmLex,vidx),2}}},
  {"cm_verbs", (const void **)&cm_verbs, &cm_verbs_n, sizeof(CmVerb), 7, {{offsetof(CmVerb,ko),4},{offsetof(CmVerb,kl),2},{offsetof(CmVerb,jo),4},{offsetof(CmVerb,jl),2},{offsetof(CmVerb,mask),2},{offsetof(CmVerb,bare_ok),1},{offsetof(CmVerb,bare_ok2),1}}},
  {"cm_parts", (const void **)&cm_parts, &cm_parts_n, sizeof(CmPart), 5, {{offsetof(CmPart,po),4},{offsetof(CmPart,pl),2},{offsetof(CmPart,dp),4},{offsetof(CmPart,dpl),2},{offsetof(CmPart,role),1}}},
  {"cm_pwords", (const void **)&cm_pwords, &cm_pwords_n, sizeof(CmMap), 4, {{offsetof(CmMap,ko),4},{offsetof(CmMap,kl),2},{offsetof(CmMap,vo),4},{offsetof(CmMap,vl),2}}},
  {"cm_yesno", (const void **)&cm_yesno, &cm_yesno_n, sizeof(CmMap), 4, {{offsetof(CmMap,ko),4},{offsetof(CmMap,kl),2},{offsetof(CmMap,vo),4},{offsetof(CmMap,vl),2}}},
  {"cm_guide", (const void **)&cm_guide, &cm_guide_n, sizeof(CmMap), 4, {{offsetof(CmMap,ko),4},{offsetof(CmMap,kl),2},{offsetof(CmMap,vo),4},{offsetof(CmMap,vl),2}}},
  {"cm_frags", (const void **)&cm_frags, &cm_frags_n, sizeof(CmStr), 2, {{offsetof(CmStr,off),4},{offsetof(CmStr,len),2}}},
  {"cm_others", (const void **)&cm_others, &cm_others_n, sizeof(CmStr), 2, {{offsetof(CmStr,off),4},{offsetof(CmStr,len),2}}},
  {"cm_role_ja", (const void **)&cm_role_ja, &cm_role_ja_n, sizeof(CmStr), 2, {{offsetof(CmStr,off),4},{offsetof(CmStr,len),2}}},
};
const int cmd_tabs_n = 12;
const unsigned long cmd_schema = CM_SCHEMA;
