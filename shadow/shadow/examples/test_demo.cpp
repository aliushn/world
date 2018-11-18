#include "demo_classification.hpp"
#include "demo_detection.hpp"

int main(int argc, char const *argv[]) {
  std::string model("models/mtcnn/mtcnn_merged.shadowmodel");
  std::string test_image("data/static/demo_2.jpg");
  std::string test_list("data/static/demo_list.txt");

  Shadow::DemoDetection demo("mtcnn");
  demo.Setup(model);
  demo.Test(test_image);
  //demo.BatchTest(test_list, false);
  //demo.CameraTest(0);

  return 0;
}
