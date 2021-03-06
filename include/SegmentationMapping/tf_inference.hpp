#pragma once

#include <tensorflow/c/c_api.h>
#include <string>
#include <memory>
#include <algorithm>
#include <cstdlib>
#include <iostream>

#include <unsupported/Eigen/CXX11/Tensor>

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

namespace SegmentationMapping {
  class tfInference {
  public:
    tfInference(const std::string & frozen_tf_graph,
                const std::string & input_tensor_name,
                const std::string & output_label_tensor_name,
                const std::string & output_distribution_tensor_name);
    
    ~tfInference() {
      TF_CloseSession(tf_sess, status);
      TF_DeleteSession(tf_sess, status);
      TF_DeleteGraph(graph_def);
      TF_DeleteStatus(status);
      
    }

    /*
      input: 
          a cv Mat rgb image

      returns: 
          cv::Mat the labeled image, 1 channel
          Eigen::MatrixXf the 3D eigen matrix, each pixel is a distribution

     */
    void segmentation(const cv::Mat & rgb, int num_class,
                      cv::Mat & label_output, cv::Mat & distribution_output,
                      bool & is_successful);



  private:
    // for tensorflow
    TF_Status * status;
    TF_Session * tf_sess;
    TF_Graph   * graph_def;
    TF_Operation * input_op;
    TF_Operation * output_label_op;
    TF_Operation * output_distribution_op;

    
    std::vector<std::int64_t> raw_network_output_shape;
    std::vector<std::int64_t> get_tensor_dims(TF_Output output) ;
  };





  /*******************************************************
   *
   *
   *               Implementations start here
   *
   *
   ****************************************************8**/

  std::vector<std::int64_t> tfInference::get_tensor_dims(TF_Output  output) {
    const TF_DataType type = TF_OperationOutputType(output);
    const int num_dims = TF_GraphGetTensorNumDims(graph_def, output, status);
    std::vector<std::int64_t> dims(num_dims);
    TF_GraphGetTensorShape(graph_def, output, dims.data(), num_dims, status);
    std::cout << " [";
    for (int d = 0; d < num_dims; ++d) {
      std::cout << dims[d];
      if (d < num_dims - 1) {
        std::cout << ", ";
      }
    }
    std::cout << "]" << std::endl;
    return dims;

  }


  static TF_Buffer* ReadBufferFromFile(const char* file) {

    auto f = std::fopen(file, "rb");
    if (f == nullptr) {
      return nullptr;
    }

    std::fseek(f, 0, SEEK_END);
    auto fsize = ftell(f);
    std::fseek(f, 0, SEEK_SET);

    if (fsize < 1) {
      std::fclose(f);
      return nullptr;
    }

    const auto data = std::malloc(fsize);
    std::fread(data, fsize, 1, f);
    std::fclose(f);

    TF_Buffer* buf = TF_NewBuffer();
    buf->data = data;
    buf->length = fsize;
    buf->data_deallocator = [](void * data_to_free, long unsigned int){ std::free(data_to_free);};
    std::cout<<"Read graph buffer "<<file<<", size "<<fsize<<std::endl;
    return buf;
  }


  tfInference::tfInference(const std::string & frozen_tf_graph,
                           const std::string & input_tensor_name,
                           const std::string & output_label_tensor_name,
                           const std::string & output_distribution_tensor_name) {
    // TF run status 
    status = TF_NewStatus();

    // read the trained frozen graph.pb file
    std::cout<<"Read graph...\n";
    TF_Buffer* buffer = ReadBufferFromFile( frozen_tf_graph.c_str() );
    graph_def = TF_NewGraph();
    TF_ImportGraphDefOptions* opts = TF_NewImportGraphDefOptions();
    TF_GraphImportGraphDef(graph_def, buffer, opts, status);
    TF_DeleteImportGraphDefOptions(opts);
    TF_DeleteBuffer(buffer);
    if (TF_GetCode(status) != TF_OK) {
      //TF_DeleteStatus(status);
      //TF_DeleteGraph(graph_def);
      std::cerr << "Can't import GraphDef" << frozen_tf_graph <<std::endl;
      return;
    }

    // assign operators for input/output
    input_op = TF_GraphOperationByName(graph_def, input_tensor_name.c_str() );
    output_label_op = TF_GraphOperationByName(graph_def, output_label_tensor_name.c_str() );
    output_distribution_op = TF_GraphOperationByName(graph_def, output_distribution_tensor_name.c_str() );
    if (!input_op || ! output_label_op || !output_distribution_op) {
      std::cerr<<" Graph Operation name for input/output does not exist\n";
      return;
    }
    
    

    // create a session, and add the graph to the session
    TF_SessionOptions* options = TF_NewSessionOptions();
    tf_sess = TF_NewSession(graph_def, options, status);
    TF_DeleteSessionOptions(options);
    if (TF_GetCode(status) != TF_OK) {
      std::cerr<< "TF Session create fails\n";
      //TF_DeleteStatus(status);
      return;
    }


    
    //TF_Output  inputs[1] = {{input_op, 0}};
    TF_Output  outputs[1] = {// {output_label_op , 0},
                              {output_distribution_op, 0} };
    //raw_network_distribution_shape = get_tensor_dims(outputs[0]);
    

    /*
    // fetch the input shape of the graph
    for (int i = 0; i < graph_def->node_size(); i++) {
      std::cout<<graph_def->node(i).name()<<std::endl;
      if (graph_def->node(i).name() == input_tensor_name ) {
        auto shape = graph_def.node(i).Get(0).attr().at("shape").shape();
        std::cout<<"Find input tensor with shape "<<shape<<std::endl;
        input_shape.resize(4); // N, H, W, C
        input_shape[0] = shape.dim(0).size();
        input_shape[1] = shape.dim(1).size();
        input_shape[2] = shape.dim(2).size();
        input_shape[3] = shape.dim(3).size();
      }

      }*/
    
    //TF_DeleteStatus(status);

  }


  static TF_Tensor* data_to_tensor(TF_DataType data_type,
                                   const std::int64_t* dims,
                                   std::size_t num_dims,
                                   const void* data,
                                   std::size_t len) {
    if (dims == nullptr) {
      return nullptr;
    }

    TF_Tensor* tensor = TF_AllocateTensor(data_type, dims, static_cast<int>(num_dims), len);
    if (tensor == nullptr) {
      return nullptr;
    }

    void* tensor_data = TF_TensorData(tensor);
    if (tensor_data == nullptr) {
      TF_DeleteTensor(tensor);
      return nullptr;
    }

    if (data != nullptr) {
      std::memcpy(tensor_data, data, std::min(len, TF_TensorByteSize(tensor)));
    }

    return tensor;
  }


  void tfInference::segmentation(const cv::Mat & rgb, int num_class,
                                 cv::Mat & label_output, cv::Mat & distribution_output,
                                 bool & is_successful) {

    cv::Mat input_img_data = rgb;
    rgb.convertTo(input_img_data, CV_32FC3);
    
    TF_Tensor* input_img_tensor[1];
    std::vector<std::int64_t> input_shape = {1, rgb.rows, rgb.cols, 3};
    input_img_tensor[0] = data_to_tensor(TF_FLOAT,
                                         input_shape.data(), input_shape.size(),
                                         input_img_data.data,
                                         input_img_data.elemSize() * input_img_data.total());
    if (input_img_tensor[0] == nullptr) {
      std::cout<<"Error: input tensor creation fails\n";
      is_successful = false;
      return ;
    }
    
    // setting up input/output 
    TF_Output  inputs[1] = {{input_op, 0}};
    TF_Output  outputs[2] = { {output_label_op , 0},
                              {output_distribution_op, 0} };

    //std::cout<<"Input data type is "<<TF_OperationOutputType(inputs[0])<<", shape is "<<input_shape[1]<<","<< input_shape[2] <<std::endl;
    //std::cout<<"Output label data type is "<<TF_OperationOutputType(outputs[0])<<", distribution data type is "<<TF_OperationOutputType(outputs[1])<<std::endl;
    
    
    // neural net inference
    TF_Tensor * output_tensors[] = {nullptr, nullptr} ;
    TF_SessionRun(tf_sess,
                  nullptr,
                  inputs,  input_img_tensor, 1,
                  outputs, output_tensors,   2,
                  nullptr, 0, nullptr, status
                  );

    if (output_tensors[0] == nullptr || output_tensors[1] == nullptr) {
      std::cerr<<"Error: Neural network infer fails\n";
      printf("Error: Status %d %s\n", TF_GetCode(status), TF_Message(status));
      TF_DeleteTensor(input_img_tensor[0]);
      is_successful = false;
      return;
    }
    //std::cout<<"Output label size is  "<<TF_TensorByteSize(output_tensors[0])<<" bytpes, # of bytes per pixel is "<<TF_TensorByteSize(output_tensors[0]) / input_shape[1] / input_shape[2] <<  std::endl;
    //std::cout<<"Output distribution size is  "<<TF_TensorByteSize(output_tensors[1])<<" bytpes, # of bytes per pixel is "<<TF_TensorByteSize(output_tensors[1]) / input_shape[1] / input_shape[2] <<  std::endl;

      
    
    int32_t * label_img_flat  = static_cast<int32_t *>(TF_TensorData(output_tensors[0]));
    float * distribution_flat = static_cast<float *>(TF_TensorData(output_tensors[1]));

    // convert to output cv mat and Eigen Mat
    cv::Mat out_label_img(input_shape[1], input_shape[2], CV_32SC1, label_img_flat);
    out_label_img.convertTo(label_output, CV_8UC1);
    cv::Mat distribution_img(input_shape[1], input_shape[2], CV_32FC(num_class), distribution_flat);
    distribution_output = distribution_img.clone();

    TF_DeleteTensor(input_img_tensor[0]);
    TF_DeleteTensor(output_tensors[0]);
    TF_DeleteTensor(output_tensors[1]);
    
    is_successful = true;
    return ;
    

  }

}


