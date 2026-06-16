/* Olal OS - motor de JavaScript proprio, escrito do zero (subconjunto real:
   numeros, strings, booleanos, var, operadores, if/while/for, funcoes com
   recursao, arrays, print/console.log). Roda dentro do kernel. */
#include "olal.h"

/* ---------- valores ---------- */
enum { T_UNDEF, T_NUM, T_BOOL, T_STR, T_FUNC, T_ARR };
typedef struct Node Node;
typedef struct Val {
    int type;
    double num;            /* NUM/BOOL */
    char *str;             /* STR */
    Node *fn;              /* FUNC (no da declaracao) */
    struct Val *items;     /* ARR */
    int len;
} Val;

/* ---------- AST ---------- */
enum { N_NUM, N_STR, N_BOOL, N_IDENT, N_BIN, N_UN, N_ASSIGN, N_CALL,
       N_IF, N_WHILE, N_FOR, N_BLOCK, N_FUNC, N_RET, N_VAR, N_ARR,
       N_INDEX, N_MEMBER, N_EXPR };
struct Node {
    int kind, op;
    double num;
    char *str;
    Node *a, *b, *c, *d;
    Node **list; int n;
};

/* ---------- saida ---------- */
static char *out_buf; static int *out_len; static int out_max;
static void emit(const char *s){ for(int i=0; s[i] && *out_len < out_max-1; i++) out_buf[(*out_len)++] = s[i]; out_buf[*out_len]=0; }
static void emit_num(double d){
    char b[40]; int k=0;
    if(d < 0){ b[k++]='-'; d=-d; }
    long ip = (long)d; double fp = d - (double)ip;
    char t[24]; int j=0; long x=ip; if(!x) t[j++]='0'; while(x){ t[j++]='0'+(x%10); x/=10; }
    while(j) b[k++]=t[--j];
    if(fp > 1e-9){ b[k++]='.'; for(int q=0;q<6 && fp>1e-9;q++){ fp*=10; int dg=(int)fp; b[k++]='0'+dg; fp-=dg; } }
    b[k]=0; emit(b);
}

/* ---------- alocacao (heap do kernel) ---------- */
static char *jstrdup(const char *s, int n){ char *p=(char*)kmalloc(n+1); for(int i=0;i<n;i++)p[i]=s[i]; p[n]=0; return p; }
static Node *newnode(int k){ Node *n=(Node*)kmalloc(sizeof(Node)); memset(n,0,sizeof(Node)); n->kind=k; return n; }

/* ---------- lexer ---------- */
static const char *src; static int pos;
static int tkind; static double tnum; static char tstr[256]; static char tid[64];
enum { TK_EOF, TK_NUM, TK_STR, TK_ID, TK_PUNCT };
static int is_sp(char c){ return c==' '||c=='\t'||c=='\n'||c=='\r'; }
static int is_d(char c){ return c>='0'&&c<='9'; }
static int is_a(char c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||c=='$'; }
static char punct[8];

static void lex(void){
    while(is_sp(src[pos])) pos++;
    /* comentarios */
    if(src[pos]=='/' && src[pos+1]=='/'){ while(src[pos] && src[pos]!='\n') pos++; lex(); return; }
    if(!src[pos]){ tkind=TK_EOF; return; }
    char c = src[pos];
    if(is_d(c) || (c=='.' && is_d(src[pos+1]))){
        double v=0; while(is_d(src[pos])){ v=v*10+(src[pos]-'0'); pos++; }
        if(src[pos]=='.'){ pos++; double f=0.1; while(is_d(src[pos])){ v+=(src[pos]-'0')*f; f*=0.1; pos++; } }
        tnum=v; tkind=TK_NUM; return;
    }
    if(c=='"' || c=='\''){
        char q=c; pos++; int k=0;
        while(src[pos] && src[pos]!=q && k<255){
            char ch=src[pos++];
            if(ch=='\\'){ char e=src[pos++]; ch = e=='n'?'\n':e=='t'?'\t':e; }
            tstr[k++]=ch;
        }
        if(src[pos]==q) pos++;
        tstr[k]=0; tkind=TK_STR; return;
    }
    if(is_a(c)){ int k=0; while(is_a(src[pos])||is_d(src[pos])){ if(k<63) tid[k++]=src[pos]; pos++; } tid[k]=0; tkind=TK_ID; return; }
    /* pontuacao (1 ou 2 chars) */
    int k=0; punct[k++]=c;
    char d=src[pos+1];
    if((c=='='&&d=='=')||(c=='!'&&d=='=')||(c=='<'&&d=='=')||(c=='>'&&d=='=')||
       (c=='&'&&d=='&')||(c=='|'&&d=='|')||(c=='+'&&d=='+')||(c=='-'&&d=='-')) punct[k++]=d;
    if(c=='='&&d=='='&&src[pos+2]=='=') punct[k++]=src[pos+2];
    punct[k]=0; pos+=k; tkind=TK_PUNCT; return;
}
static int is_punct(const char *p){ if(tkind!=TK_PUNCT) return 0; for(int i=0;p[i]||punct[i];i++) if(p[i]!=punct[i]) return 0; return 1; }
static int is_kw(const char *p){ if(tkind!=TK_ID) return 0; for(int i=0;p[i]||tid[i];i++) if(p[i]!=tid[i]) return 0; return 1; }

/* ---------- parser (descida recursiva) ---------- */
static Node *parse_expr(void);
static Node *parse_stmt(void);

static Node *parse_primary(void){
    if(tkind==TK_NUM){ Node *n=newnode(N_NUM); n->num=tnum; lex(); return n; }
    if(tkind==TK_STR){ Node *n=newnode(N_STR); n->str=jstrdup(tstr,strlen(tstr)); lex(); return n; }
    if(is_kw("true")||is_kw("false")){ Node *n=newnode(N_BOOL); n->num=is_kw("true")?1:0; lex(); return n; }
    if(is_kw("function")){
        lex(); Node *n=newnode(N_FUNC);
        if(tkind==TK_ID){ n->str=jstrdup(tid,strlen(tid)); lex(); }
        n->list=(Node**)kmalloc(sizeof(Node*)*16); n->n=0;
        lex(); /* ( */
        while(!is_punct(")") && tkind!=TK_EOF){ if(tkind==TK_ID){ Node*p=newnode(N_IDENT); p->str=jstrdup(tid,strlen(tid)); n->list[n->n++]=p; lex(); } if(is_punct(",")) lex(); }
        lex(); /* ) */
        n->a=parse_stmt(); return n;
    }
    if(is_punct("(")){ lex(); Node *e=parse_expr(); if(is_punct(")")) lex(); return e; }
    if(is_punct("[")){ lex(); Node *n=newnode(N_ARR); n->list=(Node**)kmalloc(sizeof(Node*)*64); n->n=0;
        while(!is_punct("]") && tkind!=TK_EOF){ n->list[n->n++]=parse_expr(); if(is_punct(",")) lex(); }
        lex(); return n; }
    if(tkind==TK_ID){ Node *n=newnode(N_IDENT); n->str=jstrdup(tid,strlen(tid)); lex(); return n; }
    Node *u=newnode(N_NUM); u->num=0; if(tkind!=TK_EOF) lex(); return u;
}
/* sufixos: chamada f(...), indice a[i], membro a.b */
static Node *parse_postfix(void){
    Node *e=parse_primary();
    for(;;){
        if(is_punct("(")){
            lex(); Node *c=newnode(N_CALL); c->a=e; c->list=(Node**)kmalloc(sizeof(Node*)*16); c->n=0;
            while(!is_punct(")") && tkind!=TK_EOF){ c->list[c->n++]=parse_expr(); if(is_punct(",")) lex(); }
            lex(); e=c;
        } else if(is_punct("[")){
            lex(); Node *ix=newnode(N_INDEX); ix->a=e; ix->b=parse_expr(); if(is_punct("]")) lex(); e=ix;
        } else if(is_punct(".")){
            lex(); Node *m=newnode(N_MEMBER); m->a=e; m->str=jstrdup(tid,strlen(tid)); lex(); e=m;
        } else break;
    }
    return e;
}
static Node *parse_unary(void){
    if(is_punct("!")||is_punct("-")){ char o=punct[0]; lex(); Node *n=newnode(N_UN); n->op=o; n->a=parse_unary(); return n; }
    return parse_postfix();
}
static int prec(void){
    if(is_punct("*")||is_punct("/")||is_punct("%")) return 6;
    if(is_punct("+")||is_punct("-")) return 5;
    if(is_punct("<")||is_punct(">")||is_punct("<=")||is_punct(">=")) return 4;
    if(is_punct("==")||is_punct("!=")||is_punct("===")||is_punct("!==")) return 3;
    if(is_punct("&&")) return 2;
    if(is_punct("||")) return 1;
    return 0;
}
static Node *parse_binrhs(int minp, Node *lhs){
    for(;;){
        int p=prec(); if(p<minp || p==0) return lhs;
        char op[4]; for(int i=0;i<4;i++) op[i]=punct[i];
        lex();
        Node *rhs=parse_unary();
        int p2=prec();
        if(p2>p) rhs=parse_binrhs(p+1, rhs);
        Node *b=newnode(N_BIN); for(int i=0;i<4;i++) b->str=0; b->a=lhs; b->b=rhs;
        b->num=0; /* guarda o operador em op[] via str */
        b->str=jstrdup(op,4); lhs=b;
    }
}
static Node *parse_expr(void){
    Node *lhs=parse_unary();
    /* atribuicao */
    if(is_punct("=")){ lex(); Node *a=newnode(N_ASSIGN); a->a=lhs; a->b=parse_expr(); return a; }
    return parse_binrhs(1, lhs);
}
static Node *parse_block(void){
    Node *b=newnode(N_BLOCK); b->list=(Node**)kmalloc(sizeof(Node*)*128); b->n=0;
    lex(); /* { */
    while(!is_punct("}") && tkind!=TK_EOF) b->list[b->n++]=parse_stmt();
    lex(); /* } */
    return b;
}
static Node *parse_stmt(void){
    if(is_punct("{")) return parse_block();
    if(is_kw("var")||is_kw("let")||is_kw("const")){
        lex(); Node *v=newnode(N_VAR); v->str=jstrdup(tid,strlen(tid)); lex();
        if(is_punct("=")){ lex(); v->a=parse_expr(); }
        if(is_punct(";")) lex(); return v;
    }
    if(is_kw("if")){ lex(); Node *n=newnode(N_IF); lex(); n->a=parse_expr(); if(is_punct(")")) lex();
        n->b=parse_stmt(); if(is_kw("else")){ lex(); n->c=parse_stmt(); } return n; }
    if(is_kw("while")){ lex(); Node *n=newnode(N_WHILE); lex(); n->a=parse_expr(); if(is_punct(")")) lex(); n->b=parse_stmt(); return n; }
    if(is_kw("for")){ lex(); Node *n=newnode(N_FOR); lex(); n->a=parse_stmt(); n->b=parse_expr(); if(is_punct(";")) lex(); n->c=parse_expr(); if(is_punct(")")) lex(); n->d=parse_stmt(); return n; }
    if(is_kw("return")){ lex(); Node *n=newnode(N_RET); if(!is_punct(";")&&tkind!=TK_EOF) n->a=parse_expr(); if(is_punct(";")) lex(); return n; }
    if(is_kw("function")){ Node *f=parse_primary(); return f; }
    Node *e=newnode(N_EXPR); e->a=parse_expr(); if(is_punct(";")) lex(); return e;
}

/* ---------- ambiente (escopo) ---------- */
typedef struct Var { char *name; Val val; struct Var *next; } Var;
typedef struct Env { Var *vars; struct Env *parent; } Env;
static Env *env_new(Env *p){ Env *e=(Env*)kmalloc(sizeof(Env)); e->vars=0; e->parent=p; return e; }
static Val *env_find(Env *e, const char *n){
    for(; e; e=e->parent) for(Var *v=e->vars; v; v=v->next){ int i=0; while(v->name[i]&&v->name[i]==n[i]) i++; if(!v->name[i]&&!n[i]) return &v->val; }
    return 0;
}
static void env_set(Env *e, const char *n, Val val){
    Val *f=env_find(e,n); if(f){ *f=val; return; }
    Var *v=(Var*)kmalloc(sizeof(Var)); v->name=jstrdup(n,strlen(n)); v->val=val; v->next=e->vars; e->vars=v;
}
static void env_def(Env *e, const char *n, Val val){
    Var *v=(Var*)kmalloc(sizeof(Var)); v->name=jstrdup(n,strlen(n)); v->val=val; v->next=e->vars; e->vars=v;
}

/* ---------- avaliador ---------- */
static int ctrl_ret; static Val ret_val;
static Val mkn(double d){ Val v; memset(&v,0,sizeof(v)); v.type=T_NUM; v.num=d; return v; }
static Val mkb(int b){ Val v; memset(&v,0,sizeof(v)); v.type=T_BOOL; v.num=b?1:0; return v; }
static Val mks(char *s){ Val v; memset(&v,0,sizeof(v)); v.type=T_STR; v.str=s; return v; }
static Val undef(void){ Val v; memset(&v,0,sizeof(v)); v.type=T_UNDEF; return v; }
static int truthy(Val v){ return v.type==T_NUM?v.num!=0 : v.type==T_BOOL?v.num!=0 : v.type==T_STR?v.str[0]!=0 : v.type==T_UNDEF?0:1; }

static void val_to_str(Val v, char *b, int max){
    int n=0; b[0]=0;
    if(v.type==T_STR){ for(int i=0;v.str[i]&&n<max-1;i++) b[n++]=v.str[i]; b[n]=0; }
    else if(v.type==T_BOOL){ const char*s=v.num?"true":"false"; for(int i=0;s[i];i++)b[n++]=s[i]; b[n]=0; }
    else if(v.type==T_UNDEF){ const char*s="undefined"; for(int i=0;s[i];i++)b[n++]=s[i]; b[n]=0; }
    else if(v.type==T_FUNC){ const char*s="[function]"; for(int i=0;s[i];i++)b[n++]=s[i]; b[n]=0; }
    else if(v.type==T_NUM){
        double d=v.num; if(d<0){ b[n++]='-'; d=-d; }
        long ip=(long)d; double fp=d-(double)ip; char t[24]; int j=0; long x=ip; if(!x)t[j++]='0'; while(x){t[j++]='0'+(x%10);x/=10;} while(j)b[n++]=t[--j];
        if(fp>1e-9){ b[n++]='.'; for(int q=0;q<6&&fp>1e-9;q++){ fp*=10; int dg=(int)fp; b[n++]='0'+dg; fp-=dg; } }
        b[n]=0;
    } else if(v.type==T_ARR){
        b[n++]='['; for(int i=0;i<v.len && n<max-8;i++){ char t[48]; val_to_str(v.items[i],t,48); for(int k=0;t[k];k++)b[n++]=t[k]; if(i<v.len-1)b[n++]=','; } b[n++]=']'; b[n]=0;
    }
}

static Val eval(Node *nd, Env *env);
static void exec(Node *nd, Env *env);

static Val call_fn(Val f, Val *args, int na, Env *env){
    if(f.type!=T_FUNC) return undef();
    Node *fn=f.fn; Env *fe=env_new(env);
    for(int i=0;i<fn->n;i++){ Val a = i<na?args[i]:undef(); env_def(fe, fn->list[i]->str, a); }
    ctrl_ret=0; ret_val=undef();
    exec(fn->a, fe);
    int r=ctrl_ret; ctrl_ret=0;
    return r? ret_val : undef();
}

static Val eval(Node *nd, Env *env){
    if(!nd) return undef();
    switch(nd->kind){
        case N_NUM: return mkn(nd->num);
        case N_STR: return mks(nd->str);
        case N_BOOL: return mkb((int)nd->num);
        case N_ARR: { Val v; memset(&v,0,sizeof(v)); v.type=T_ARR; v.len=nd->n; v.items=(Val*)kmalloc(sizeof(Val)*(nd->n+1)); for(int i=0;i<nd->n;i++) v.items[i]=eval(nd->list[i],env); return v; }
        case N_FUNC: { Val v; memset(&v,0,sizeof(v)); v.type=T_FUNC; v.fn=nd; if(nd->str) env_def(env,nd->str,v); return v; }
        case N_IDENT: { Val *p=env_find(env,nd->str); return p?*p:undef(); }
        case N_INDEX: { Val a=eval(nd->a,env); Val i=eval(nd->b,env); if(a.type==T_ARR){ int idx=(int)i.num; if(idx>=0&&idx<a.len) return a.items[idx]; } return undef(); }
        case N_MEMBER: { Val a=eval(nd->a,env); if(a.type==T_ARR && nd->str[0]=='l') return mkn(a.len); if(a.type==T_STR && nd->str[0]=='l'){ return mkn(strlen(a.str)); } return undef(); }
        case N_UN: { Val a=eval(nd->a,env); if(nd->op=='!') return mkb(!truthy(a)); return mkn(-a.num); }
        case N_ASSIGN: { Val v=eval(nd->b,env);
            if(nd->a->kind==N_IDENT) env_set(env,nd->a->str,v);
            else if(nd->a->kind==N_INDEX){ Val arr=eval(nd->a->a,env); int idx=(int)eval(nd->a->b,env).num; if(arr.type==T_ARR && idx>=0 && idx<arr.len) arr.items[idx]=v; }
            return v; }
        case N_BIN: {
            const char *o=nd->str;
            if(o[0]=='&'&&o[1]=='&'){ Val a=eval(nd->a,env); return truthy(a)?eval(nd->b,env):a; }
            if(o[0]=='|'&&o[1]=='|'){ Val a=eval(nd->a,env); return truthy(a)?a:eval(nd->b,env); }
            Val a=eval(nd->a,env), b=eval(nd->b,env);
            if(o[0]=='+' && (a.type==T_STR||b.type==T_STR)){ char *r=(char*)kmalloc(600); char t[300]; val_to_str(a,t,300); int k=0; for(int i=0;t[i];i++)r[k++]=t[i]; val_to_str(b,t,300); for(int i=0;t[i];i++)r[k++]=t[i]; r[k]=0; return mks(r); }
            double x=a.num, y=b.num;
            if(o[0]=='+'&&!o[1]) return mkn(x+y);
            if(o[0]=='-'&&!o[1]) return mkn(x-y);
            if(o[0]=='*') return mkn(x*y);
            if(o[0]=='/') return mkn(y!=0?x/y:0);
            if(o[0]=='%'){ long r=(long)x%((long)y?(long)y:1); return mkn(r); }
            if(o[0]=='<'&&!o[1]) return mkb(x<y);
            if(o[0]=='>'&&!o[1]) return mkb(x>y);
            if(o[0]=='<'&&o[1]=='=') return mkb(x<=y);
            if(o[0]=='>'&&o[1]=='=') return mkb(x>=y);
            if(o[0]=='='&&o[1]=='='){ if(a.type==T_STR&&b.type==T_STR){ int i=0; while(a.str[i]&&a.str[i]==b.str[i])i++; return mkb(a.str[i]==b.str[i]); } return mkb(x==y); }
            if(o[0]=='!'&&o[1]=='='){ if(a.type==T_STR&&b.type==T_STR){ int i=0; while(a.str[i]&&a.str[i]==b.str[i])i++; return mkb(a.str[i]!=b.str[i]); } return mkb(x!=y); }
            return undef();
        }
        case N_CALL: {
            /* builtins: print(...), console.log(...) */
            Node *callee=nd->a;
            int is_log = 0;
            if(callee->kind==N_IDENT && callee->str[0]=='p' && callee->str[1]=='r') is_log=1;        /* print */
            if(callee->kind==N_MEMBER && callee->str[0]=='l') is_log=1;                                /* .log */
            if(callee->kind==N_MEMBER && callee->str[0]=='p'){ /* arr.push */
                Val arr=eval(callee->a,env); if(arr.type==T_ARR){ Val v=eval(nd->list[0],env); arr.items[arr.len]=v; /* nota: sem realocar */ }
            }
            if(is_log){
                for(int i=0;i<nd->n;i++){ Val v=eval(nd->list[i],env); char t[400]; val_to_str(v,t,400); emit(t); if(i<nd->n-1) emit(" "); }
                emit("\n"); return undef();
            }
            Val f=eval(callee,env);
            Val args[16]; int na=nd->n>16?16:nd->n;
            for(int i=0;i<na;i++) args[i]=eval(nd->list[i],env);
            return call_fn(f,args,na,env);
        }
        default: return undef();
    }
}

static void exec(Node *nd, Env *env){
    if(!nd || ctrl_ret) return;
    switch(nd->kind){
        case N_BLOCK: for(int i=0;i<nd->n && !ctrl_ret;i++) exec(nd->list[i],env); break;
        case N_VAR: env_def(env, nd->str, nd->a?eval(nd->a,env):undef()); break;
        case N_EXPR: eval(nd->a,env); break;
        case N_IF: if(truthy(eval(nd->a,env))) exec(nd->b,env); else if(nd->c) exec(nd->c,env); break;
        case N_WHILE: { int guard=0; while(truthy(eval(nd->a,env)) && !ctrl_ret && guard++<2000000) exec(nd->b,env); break; }
        case N_FOR: { exec(nd->a,env); int guard=0; while(truthy(eval(nd->b,env)) && !ctrl_ret && guard++<2000000){ exec(nd->d,env); eval(nd->c,env); } break; }
        case N_RET: ret_val = nd->a?eval(nd->a,env):undef(); ctrl_ret=1; break;
        case N_FUNC: eval(nd,env); break;
        default: eval(nd,env); break;
    }
}

/* roda o programa JS em 'code', escreve a saida em 'out' */
void js_run(const char *code, char *out, int max){
    out_buf=out; static int len; len=0; out_len=&len; out_max=max; out[0]=0;
    src=code; pos=0; ctrl_ret=0;
    Env *g=env_new(0);
    lex();
    int guard=0;
    while(tkind!=TK_EOF && guard++<10000){
        Node *s=parse_stmt();
        exec(s,g);
        if(ctrl_ret) break;
    }
}
