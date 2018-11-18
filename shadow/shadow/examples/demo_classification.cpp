#include "demo_classification.hpp"

namespace Shadow {

void DemoClassification::Test(const std::string &image_file) {
  im_ini_.Read(image_file);
  timer_.start();
  Predict(im_ini_, RectF(0, 0, im_ini_.w_, im_ini_.h_), &scores_);
  LOG(INFO) << "Predicted in " << timer_.get_millisecond() << " ms";
  for (const auto &it : scores_) {
    const auto &score = it.second;
    const auto &max_k = Util::top_k(score, 1);
    std::stringstream ss;
    ss << it.first << ": ";
    for (const auto idx : max_k) {
      ss << idx << " (" << score[idx] << ") ";
    }
    LOG(INFO) << ss.str();
  }
}

void DemoClassification::BatchTest(const std::string &list_file) {
  const auto &image_list = Util::load_list(list_file);
  int num_im = static_cast<int>(image_list.size()), count = 0;
  double time_cost = 0;
  Process process(20, num_im, "Processing: ");
  const auto &result_file = Util::find_replace_last(list_file, ".", "-result.");
  std::ofstream file(result_file);
  CHECK(file.is_open()) << "Can't open file " << result_file;
  for (const auto &im_path : image_list) {
    im_ini_.Read(im_path);
    timer_.start();
    Predict(im_ini_, RectF(0, 0, im_ini_.w_, im_ini_.h_), &scores_);
    time_cost += timer_.get_millisecond();
    PrintDetections(im_path, scores_, &file);
    process.update(count++, &std::cout);
  }
  file.close();
  LOG(INFO) << "Processed in: " << time_cost
            << " ms, each frame: " << time_cost / num_im << " ms";
}

void DemoClassification::PrintDetections(
    const std::string &im_name, const std::map<std::string, VecFloat> &scores,
    std::ostream *os) {
  *os << im_name << ":" << std::endl;
  for (const auto &it : scores) {
    const auto &score = it.second;
    const auto &max_k = Util::top_k(score, 1);
    *os << it.first << ": ";
    for (const auto idx : max_k) {
      *os << idx << " (" << score[idx] << ") ";
    }
    *os << std::endl;
  }
  *os << "-------------------------" << std::endl;
}

}  // namespace Shadow
