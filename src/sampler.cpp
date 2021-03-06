
#include "sampler.h"

#include "mcmc_utils.h"

#include <Rcpp.h>
#include <Rmath.h>
#include <algorithm>
#include <random>

std::random_device Sampler::rd;

Sampler::Sampler(Lookup lookup) : lookup(lookup)
{
    eng = std::ranlux24_base(rd());
    gsl_rd = gsl_rng_alloc(gsl_rng_minstd);
    unif_distr = std::uniform_real_distribution<double>(0, 1);
    ber_distr = std::bernoulli_distribution(.5);
}

double Sampler::dbeta(double x, double alpha, double beta, bool return_log)
{
    return R::dbeta(x, alpha, beta, return_log);
}

double Sampler::dpois(int x, double mean, bool return_log)
{
    return R::dpois(x, mean, return_log);
}

double Sampler::dztpois(int x, double lambda)
{
    return x * std::log(lambda) - std::log(std::exp(lambda) - 1) -
           lookup.lookup_lgamma[x + 1];
}

double Sampler::dgamma(double x, double shape, double scale, bool return_log)
{
    return R::dgamma(x, shape, scale, return_log);
}

double Sampler::rgamma(double alpha, double beta)
{
    gamma_distr.param(std::gamma_distribution<double>::param_type(alpha, beta));
    double x = gamma_distr(eng);

    if (x < UNDERFLO)
    {
        x = UNDERFLO;
    }
    else if (x > OVERFLO)
    {
        x = OVERFLO;
    }

    return x;
};

double Sampler::rgamma2(double shape, double rate)
{
    return R::rgamma(shape, 1 / rate);
}

std::vector<double> Sampler::rdirichlet(std::vector<double> const &shape_vec)
{
    int n = shape_vec.size();
    std::vector<double> res(n);

    double res_sum = 0;
    for (int i = 0; i < n; i++)
    {
        res[i] = rgamma(shape_vec[i], 1.0);
        res_sum += res[i];
    }

    double res_sum_inv = 1.0 / res_sum;
    for (size_t i = 0; i < res.size(); i++)
    {
        res[i] *= res_sum_inv;
    }

    return res;
};

std::vector<double> Sampler::rlogit_norm(std::vector<double> const &p,
                                         double variance)
{
    int n = p.size() - 1;

    std::vector<double> ret(n + 1);

    double tmp1 = 0;
    for (int i = 0; i < n; i++)
    {
        norm_distr.param(std::normal_distribution<double>::param_type(
            log(p[i] / p[n]), variance));
        ret[i] = exp(norm_distr(eng));
        tmp1 += ret[i];
    }

    double tmp2 = 1.0 / (1.0 + tmp1);
    for (int i = 0; i < n; i++)
    {
        ret[i] *= tmp2;
    }

    ret[n] = tmp2;

    return ret;
}

double Sampler::sample_mean_coi(double mean_shape, double mean_rate)
{
    return rgamma2(mean_shape, mean_rate) + 1;
}

int Sampler::sample_random_int(int lower, int upper)
{
    unif_int_distr.param(
        std::uniform_int_distribution<>::param_type(lower, upper));
    return unif_int_distr(eng);
}

double Sampler::get_coi_log_prob(int coi, double mean)
{
    return dztpois(coi, mean);
}

double Sampler::get_coi_mean_log_prior(double mean, double shape, double scale)
{
    return dgamma(mean, shape, scale, true);
}

int Sampler::sample_coi_delta() { return (2 * ber_distr(eng) - 1); }

int Sampler::sample_coi_delta(double coi_prop_mean)
{
    geom_distr.param(std::geometric_distribution<int>::param_type(
        1.0 / (1.0 + coi_prop_mean)));
    // abs delta >= 1
    return (2 * ber_distr(eng) - 1) * (geom_distr(eng));
}

double Sampler::get_epsilon_log_prior(double x, double alpha, double beta)
{
    return dbeta(x, alpha, beta, true);
}

// double Sampler::sample_epsilon(double curr_epsilon, double variance) {
//     norm_distr.param(std::normal_distribution<double>::param_type(log(curr_epsilon
//     / (1 - curr_epsilon)), variance)); double prop = norm_distr(eng); return
//     exp(prop) / (1 + exp(prop));
// };

double Sampler::sample_epsilon(double curr_epsilon, double variance)
{
    norm_distr.param(
        std::normal_distribution<double>::param_type(curr_epsilon, variance));
    double prop = norm_distr(eng);
    return prop;
};

double Sampler::sample_epsilon_pos(double curr_epsilon_pos, double variance)
{
    return sample_epsilon(curr_epsilon_pos, variance);
};

double Sampler::sample_epsilon_neg(double curr_epsilon_neg, double variance)
{
    return sample_epsilon(curr_epsilon_neg, variance);
};

std::vector<double> Sampler::sample_allele_frequencies(
    std::vector<double> const &curr_allele_frequencies, double alpha)
{
    std::vector<double> shape_vec(curr_allele_frequencies.size());

    for (size_t i = 0; i < shape_vec.size(); i++)
    {
        shape_vec[i] = curr_allele_frequencies[i] * alpha;
    }

    return rdirichlet(shape_vec);
};

std::vector<double> Sampler::sample_allele_frequencies2(
    std::vector<double> const &curr_allele_frequencies, double variance)
{
    return rlogit_norm(curr_allele_frequencies, variance);
};

std::vector<int> Sampler::sample_latent_genotype(
    int coi, const std::vector<double> &allele_frequencies)
{
    std::vector<int> tmp_alleles(allele_frequencies.size(), 0);
    std::vector<int> allele_index_vec{};
    allele_index_vec.reserve(coi);

    gsl_ran_multinomial(gsl_rd, allele_frequencies.size(), coi,
                        allele_frequencies.data(),
                        (unsigned int *)tmp_alleles.data());

    for (size_t i = 0; i < allele_frequencies.size(); i++)
    {
        if (tmp_alleles[i] > 0)
        {
            allele_index_vec.push_back(i);
        }
    }
    return allele_index_vec;
}

double Sampler::sample_log_mh_acceptance() { return log(unif_distr(eng)); };
