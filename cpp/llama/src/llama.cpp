#include "kvm_api.hpp"

#include <cassert>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <sys/random.h>

#include "common/common.h"
#include "common/grammar-parser.h"
#include "llama.h"
#include "build-info.h"

static int initialize_llama(bool compute, int n);
static std::string exec_llama(std::string prompt);
static gpt_params params;
static llama_context ** g_ctx;
static llama_model * model;
static llama_context * ctx;
static llama_context * ctx_guidance;

static void
on_get(const char *prompt, const char *neg_prompt)
{
	auto result = exec_llama(prompt);
	backend_response_str(200, "text/plain", result.c_str());
}

static void
on_post(const char *url, const char *arg, const char *ctype, const uint8_t *data, size_t len)
{
	std::string prompt((const char *)data, len);
	auto result = exec_llama(std::move(prompt));
	backend_response_str(200, "text/plain", result.c_str());
}

int main(int argc, char** argv)
{
	if (IS_LINUX_MAIN())
	{
		/*
		if (gpt_params_parse(argc, argv, params) == false) {
			return 1;
		}
		initialize_llama(false, 60);
		*/
		initialize_llama(true, 60);

		auto result = exec_llama("Hey, how are you?");
		printf("Result: \"%s\"\n", result.c_str());
		return 0;
	}

	if (strcmp(argv[2], "request") == 0)
	{
		initialize_llama(true, 60);
		fflush(stdout);
	}

	set_backend_get(on_get);
	set_backend_post(on_post);
	wait_for_requests();
}

int initialize_llama(bool compute, int n)
{
	if (compute)
	{
		params.model = "/tmp/llama.f16";
		params.n_threads = 1;
		params.use_mmap  = false;
		params.n_predict = n;
	}

    if (params.rope_freq_base != 10000.0) {
        fprintf(stderr, "%s: warning: changing RoPE frequency base to %g (default 10000.0)\n", __func__, params.rope_freq_base);
    }

    if (params.rope_freq_scale != 1.0) {
        fprintf(stderr, "%s: warning: scaling RoPE frequency by %g (default 1.0)\n", __func__, params.rope_freq_scale);
    }

    if (params.n_ctx > 2048) {
        // TODO: determine the actual max context of the model (e.g. 4096 for LLaMA v2) and use that instead of 2048
        fprintf(stderr, "%s: warning: base model only supports context sizes no greater than 2048 tokens (%d specified)\n", __func__, params.n_ctx);
    } else if (params.n_ctx < 8) {
        fprintf(stderr, "%s: warning: minimum context size is 8, using minimum size.\n", __func__);
        params.n_ctx = 8;
    }

    fprintf(stderr, "%s: build = %d (%s)\n", __func__, BUILD_NUMBER, BUILD_COMMIT);

    llama_backend_init(params.numa);

    g_ctx = &ctx;

    // load the model and apply lora adapter, if any
    std::tie(model, ctx) = llama_init_from_gpt_params(params);
    if (params.cfg_scale > 1.f) {
        struct llama_context_params lparams = llama_context_params_from_gpt_params(params);
        ctx_guidance = llama_new_context_with_model(model, lparams);
    }

    if (model == NULL) {
        fprintf(stderr, "%s: error: unable to load model\n", __func__);
        return 1;
    }

    const int n_ctx_train = llama_n_ctx_train(ctx);
    if (params.n_ctx > n_ctx_train) {
        fprintf(stderr, "%s: warning: model was trained on only %d context tokens (%d specified)\n",
                __func__, n_ctx_train, params.n_ctx);
    } else if (params.n_ctx < 8) {
        fprintf(stderr, "%s: warning: minimum context size is 8, using minimum size.\n", __func__);
        params.n_ctx = 8;
    }

    // print system information
    {
        fprintf(stderr, "\n");
        fprintf(stderr, "system_info: n_threads = %d / %d | %s\n",
                params.n_threads, std::thread::hardware_concurrency(), llama_print_system_info());
    }

    return 0;
}

std::string exec_llama(std::string input_prompt)
{
	params.prompt = std::move(input_prompt);
	std::string result;

	getrandom(&params.seed, sizeof(params.seed), 0);

	params.seed = 2446600456;
    fprintf(stderr, "%s: seed  = %u\n", __func__, params.seed);

	llama_set_rng_seed(ctx, params.seed);

    const bool add_bos = llama_vocab_type(ctx) == LLAMA_VOCAB_TYPE_SPM;

    // tokenize the prompt
    std::vector<llama_token> embd_inp;

	embd_inp = ::llama_tokenize(ctx, params.prompt, add_bos);

    // Tokenize negative prompt
    std::vector<llama_token> guidance_inp;
    int guidance_offset = 0;
    int original_prompt_len = 0;
    if (ctx_guidance) {
        params.cfg_negative_prompt.insert(0, 1, ' ');
        guidance_inp = ::llama_tokenize(ctx_guidance, params.cfg_negative_prompt, add_bos);

        std::vector<llama_token> original_inp = ::llama_tokenize(ctx, params.prompt, add_bos);
        original_prompt_len = original_inp.size();
        guidance_offset = (int)guidance_inp.size() - original_prompt_len;
    }

    const int n_ctx = llama_n_ctx(ctx);

    if ((int) embd_inp.size() > n_ctx - 4) {
        fprintf(stderr, "%s: error: prompt is too long (%d tokens, max %d)\n", __func__, (int) embd_inp.size(), n_ctx - 4);
        return "";
    }

    // number of tokens to keep when resetting context
    if (params.n_keep < 0 || params.n_keep > (int) embd_inp.size() || params.instruct) {
        params.n_keep = (int)embd_inp.size();
    }

    // prefix & suffix for instruct mode
    const auto inp_pfx = ::llama_tokenize(ctx, "\n\n### Instruction:\n\n", add_bos);
    const auto inp_sfx = ::llama_tokenize(ctx, "\n\n### Response:\n\n", false);

    // in instruct mode, we inject a prefix and a suffix to each input by the user
    if (params.instruct) {
        params.antiprompt.push_back("### Instruction:\n\n");
    }

    if (params.verbose_prompt) {
        fprintf(stderr, "\n");
        fprintf(stderr, "%s: prompt: '%s'\n", __func__, params.prompt.c_str());
        fprintf(stderr, "%s: number of tokens in prompt = %zu\n", __func__, embd_inp.size());
        for (int i = 0; i < (int) embd_inp.size(); i++) {
            fprintf(stderr, "%6d -> '%s'\n", embd_inp[i], llama_token_to_piece(ctx, embd_inp[i]).c_str());
        }

        if (ctx_guidance) {
            fprintf(stderr, "\n");
            fprintf(stderr, "%s: negative prompt: '%s'\n", __func__, params.cfg_negative_prompt.c_str());
            fprintf(stderr, "%s: number of tokens in negative prompt = %zu\n", __func__, guidance_inp.size());
            for (int i = 0; i < (int) guidance_inp.size(); i++) {
                fprintf(stderr, "%6d -> '%s'\n", guidance_inp[i], llama_token_to_piece(ctx, guidance_inp[i]).c_str());
            }
        }

        if (params.n_keep > 0) {
        fprintf(stderr, "%s: static prompt based on n_keep: '", __func__);
            for (int i = 0; i < params.n_keep; i++) {
                fprintf(stderr, "%s", llama_token_to_piece(ctx, embd_inp[i]).c_str());
            }
            fprintf(stderr, "'\n");
        }
        fprintf(stderr, "\n");
    }

    fprintf(stderr, "sampling: repeat_last_n = %d, repeat_penalty = %f, presence_penalty = %f, frequency_penalty = %f, top_k = %d, tfs_z = %f, top_p = %f, typical_p = %f, temp = %f, mirostat = %d, mirostat_lr = %f, mirostat_ent = %f\n",
            params.repeat_last_n, params.repeat_penalty, params.presence_penalty, params.frequency_penalty, params.top_k, params.tfs_z, params.top_p, params.typical_p, params.temp, params.mirostat, params.mirostat_eta, params.mirostat_tau);
    fprintf(stderr, "generate: n_ctx = %d, n_batch = %d, n_predict = %d, n_keep = %d\n", n_ctx, params.n_batch, params.n_predict, params.n_keep);
    fprintf(stderr, "\n\n");

    grammar_parser::parse_state parsed_grammar;
    llama_grammar *             grammar = NULL;

    if (!params.grammar.empty()) {
        parsed_grammar = grammar_parser::parse(params.grammar.c_str());
        // will be empty (default) if there are parse errors
        if (parsed_grammar.rules.empty()) {
            return "";
        }
        fprintf(stderr, "%s: grammar:\n", __func__);
        grammar_parser::print_grammar(stderr, parsed_grammar);
        fprintf(stderr, "\n");

        {
            auto it = params.logit_bias.find(llama_token_eos(ctx));
            if (it != params.logit_bias.end() && it->second == -INFINITY) {
                fprintf(stderr,
                    "%s: warning: EOS token is disabled, which will cause most grammars to fail\n", __func__);
            }
        }

        std::vector<const llama_grammar_element *> grammar_rules(parsed_grammar.c_rules());
        grammar = llama_grammar_init(
            grammar_rules.data(), grammar_rules.size(), parsed_grammar.symbol_ids.at("root"));
    }

    // TODO: replace with ring-buffer
    std::vector<llama_token> last_n_tokens(n_ctx);
    std::fill(last_n_tokens.begin(), last_n_tokens.end(), 0);

    bool is_antiprompt       = false;
    int  n_past              = 0;
    int  n_remain            = params.n_predict;
    int  n_consumed          = 0;
    int  n_past_guidance     = 0;

    std::vector<llama_token> embd;
    std::vector<llama_token> embd_guidance;

    std::vector<llama_token> last_tokens(n_ctx);
    std::fill(last_tokens.begin(), last_tokens.end(), 0);

    const int n_vocab = llama_n_vocab(ctx);

    std::vector<llama_token_data> candidates;
    candidates.reserve(n_vocab);

    while (n_remain != 0 && !is_antiprompt) {
        // predict
        if (embd.size() > 0) {
            // Note: n_ctx - 4 here is to match the logic for commandline prompt handling via
            // --prompt or --file which uses the same value.
            auto max_embd_size = n_ctx - 4;
            // Ensure the input doesn't exceed the context size by truncating embd if necessary.
            if ((int)embd.size() > max_embd_size) {
                auto skipped_tokens = embd.size() - max_embd_size;
                printf("<<input too long: skipped %zu token%s>>", skipped_tokens, skipped_tokens != 1 ? "s" : "");
                fflush(stdout);
                embd.resize(max_embd_size);
            }

            // infinite text generation via context swapping
            // if we run out of context:
            // - take the n_keep first tokens from the original prompt (via n_past)
            // - take half of the last (n_ctx - n_keep) tokens and recompute the logits in batches
            if (n_past + (int) embd.size() + std::max<int>(0, guidance_offset) > n_ctx) {
                if (params.n_predict == -2) {
                    fprintf(stderr, "\n\n%s: context full, stopping generation\n", __func__);
                    break;
                }

                const int n_left = n_past - params.n_keep;
                // always keep the first token - BOS
                n_past = std::max(1, params.n_keep);
                n_past_guidance = std::max(1, params.n_keep + guidance_offset);

                // insert n_left/2 tokens at the start of embd from last_n_tokens
                embd.insert(embd.begin(), last_n_tokens.begin() + n_ctx - n_left/2 - embd.size(), last_n_tokens.end() - embd.size());

                //printf("\n---\n");
                //printf("resetting: '");
                //for (int i = 0; i < (int) embd.size(); i++) {
                //    printf("%s", llama_token_to_piece(ctx, embd[i]));
                //}
                //printf("'\n");
                //printf("\n---\n");
            }

            // evaluate tokens in batches
            // embd is typically prepared beforehand to fit within a batch, but not always

            if (ctx_guidance) {
                int input_size = 0;
                llama_token * input_buf = NULL;

                if (n_past_guidance < (int) guidance_inp.size()) {
                    // Guidance context should have the same data with these modifications:
                    //
                    // * Replace the initial prompt
                    // * Shift everything by guidance_offset
                    embd_guidance = guidance_inp;
                    if (embd.begin() + original_prompt_len < embd.end()) {
                        embd_guidance.insert(
                            embd_guidance.end(),
                            embd.begin() + original_prompt_len,
                            embd.end()
                        );
                    }

                    input_buf  = embd_guidance.data();
                    input_size = embd_guidance.size();
                } else {
                    input_buf  = embd.data();
                    input_size = embd.size();
                }

                for (int i = 0; i < input_size; i += params.n_batch) {
                    int n_eval = std::min(input_size - i, params.n_batch);
                    if (llama_eval(ctx_guidance, input_buf + i, n_eval, n_past_guidance, params.n_threads)) {
                        LOG_TEE("%s : failed to eval\n", __func__);
                        return "";
                    }

                    n_past_guidance += n_eval;
                }
            }

            for (int i = 0; i < (int) embd.size(); i += params.n_batch) {
                int n_eval = (int) embd.size() - i;
                if (n_eval > params.n_batch) {
                    n_eval = params.n_batch;
                }

                if (llama_eval(ctx, &embd[i], n_eval, n_past, params.n_threads)) {
                    LOG_TEE("%s : failed to eval\n", __func__);
                    return "";
                }

                n_past += n_eval;
            }
        }

        embd.clear();
        embd_guidance.clear();

        if ((int) embd_inp.size() <= n_consumed) {

            const llama_token id = llama_sample_token(ctx, ctx_guidance, grammar, params, last_tokens, candidates);

            last_tokens.erase(last_tokens.begin());
            last_tokens.push_back(id);

            embd.push_back(id);

            // decrement remaining sampling budget
            --n_remain;

        } else {
            // some user input remains from prompt or interaction, forward it to processing
            while ((int) embd_inp.size() > n_consumed) {
                embd.push_back(embd_inp[n_consumed]);
                last_tokens.erase(last_tokens.begin());
                last_tokens.push_back(embd_inp[n_consumed]);
                ++n_consumed;
                if ((int) embd.size() >= params.n_batch) {
                    break;
                }
            }
        }

        // result text
		for (auto id : embd) {
			const auto word = llama_token_to_piece(ctx, id);
			//fprintf(stderr, "%s", word.c_str());
			result.append(std::move(word));
		}

        // end of text token
        if (!embd.empty() && embd.back() == llama_token_eos(ctx)) {
            fprintf(stderr, " [end of text]\n");
            break;
        }
    }

    llama_print_timings(ctx);
	return result;
}
