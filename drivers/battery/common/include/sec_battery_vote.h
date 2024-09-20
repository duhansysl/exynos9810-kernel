
#ifndef __SEC_VOTER_H
#define __SEC_VOTER_H __FILE__

enum {
	SEC_VOTE_MIN,
	SEC_VOTE_MAX,
	SEC_VOTE_EN,
};

struct sec_vote;

extern int get_sec_vote(struct sec_vote * vote, const char ** name, int * value);
extern struct sec_vote * sec_vote_init(const char * name, int type, int num, int init_val,
		const char ** voter_name, int(*cb)(void *data, int value), void * data);
extern void sec_vote_destory(struct sec_vote * vote);
extern void sec_vote(struct sec_vote * vote, int event, int en, int value);
extern void sec_vote_refresh(struct sec_vote * vote);
extern const char * get_sec_keyvoter_name(struct sec_vote * vote);
extern int get_sec_vote_result(struct sec_vote *vote);
extern int get_sec_voter_status(struct sec_vote *vote, int id, int * v);

#endif
