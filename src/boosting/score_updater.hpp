#ifndef LIGHTGBDT_BOOSTING_SCORE_UPDATER_HPP_
#define LIGHTGBDT_BOOSTING_SCORE_UPDATER_HPP_


#include <LightGBDT/utils/openmp_wrapper.h>
#include <LightGBDT/meta.h>
#include <LightGBDT/dataset.h>
#include <LightGBDT/tree.h>
#include <LightGBDT/tree_learner.h>

#include <cstring>

namespace LightGBDT {
/*!
* \brief Used to store and update score for data
*/
class ScoreUpdater {
public:
  /*!
  * \brief Constructor, will pass a const pointer of dataset
  * \param data This class will bind with this data set
  */
  ScoreUpdater(const Dataset* data, int num_tree_per_iteration) : data_(data) {
    num_data_ = data->num_data();
    int64_t total_size = static_cast<int64_t>(num_data_) * num_tree_per_iteration;
    score_.resize(total_size);
    // default start score is zero
    #pragma omp parallel for schedule(static)
    for (int64_t i = 0; i < total_size; ++i) {
      score_[i] = 0.0f;
    }
    has_init_score_ = false;
    const double* init_score = data->metadata().init_score();
    // if exists initial score, will start from it
    if (init_score != nullptr) {
      if ((data->metadata().num_init_score() % num_data_) != 0
          || (data->metadata().num_init_score() / num_data_) != num_tree_per_iteration) {
        Log::Fatal("number of class for initial score error");
      }
      has_init_score_ = true;
      #pragma omp parallel for schedule(static)
      for (int64_t i = 0; i < total_size; ++i) {
        score_[i] = init_score[i];
      }
    }
  }
  /*! \brief Destructor */
  ~ScoreUpdater() {

  }

  inline bool has_init_score() const { return has_init_score_; }

  inline void AddScore(double val, int cur_tree_id) {
    int64_t offset = cur_tree_id * num_data_;
    #pragma omp parallel for schedule(static)
    for (int64_t i = 0; i < num_data_; ++i) {
      score_[offset + i] += val;
    }
  }
  /*!
  * \brief Using tree model to get prediction number, then adding to scores for all data
  *        Note: this function generally will be used on validation data too.
  * \param tree Trained tree model
  * \param cur_tree_id Current tree for multiclass training
  */
  inline void AddScore(const Tree* tree, int cur_tree_id) {
    tree->AddPredictionToScore(data_, num_data_, score_.data() + cur_tree_id * num_data_);
  }
  /*!
  * \brief Adding prediction score, only used for training data.
  *        The training data is partitioned into tree leaves after training
  *        Based on which We can get prediction quickly.
  * \param tree_learner
  * \param cur_tree_id Current tree for multiclass training
  */
  inline void AddScore(const TreeLearner* tree_learner, const Tree* tree, int cur_tree_id) {
    tree_learner->AddPredictionToScore(tree, score_.data() + cur_tree_id * num_data_);
  }
  /*!
  * \brief Using tree model to get prediction number, then adding to scores for parts of data
  *        Used for prediction of training out-of-bag data
  * \param tree Trained tree model
  * \param data_indices Indices of data that will be processed
  * \param data_cnt Number of data that will be processed
  * \param cur_tree_id Current tree for multiclass training
  */
  inline void AddScore(const Tree* tree, const data_size_t* data_indices,
                       data_size_t data_cnt, int cur_tree_id) {
    tree->AddPredictionToScore(data_, data_indices, data_cnt, score_.data() + cur_tree_id * num_data_);
  }
  /*! \brief Pointer of score */
  inline const double* score() const { return score_.data(); }
  inline const data_size_t num_data() const { return num_data_; }

  /*! \brief Disable copy */
  ScoreUpdater& operator=(const ScoreUpdater&) = delete;
  /*! \brief Disable copy */
  ScoreUpdater(const ScoreUpdater&) = delete;
private:
  /*! \brief Number of total data */
  data_size_t num_data_;
  /*! \brief Pointer of data set */
  const Dataset* data_;
  /*! \brief Scores for data set */
  std::vector<double> score_;
  bool has_init_score_;
};

}  // namespace LightGBDT
#endif   // LightGBDT_BOOSTING_SCORE_UPDATER_HPP_