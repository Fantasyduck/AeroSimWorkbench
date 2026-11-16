# 机器学习后端

`aerosim_machine_learning` 为机器学习算法提供独立的 C++14 模块，当前实现 RBF 核岭回归，依赖 Eigen 3。公开接口使用标准库容器，训练和预测均可在无界面程序中使用。

## 接口

```cpp
#include <aerosim/machine_learning/RbfKrr.h>

aerosim::machine_learning::RbfKrr model;
aerosim::machine_learning::RbfKrrOptions options;
options.gamma = 0.1;
options.regularization = 1e-6;
std::string error;
if (model.fit({{-1, -1}, {-1, 1}, {1, -1}, {1, 1}},
              {1, -1, -1, 1}, options, &error)) {
    double prediction;
    model.predict({0.5, -0.5}, &prediction, &error);
}
```

每个训练样本为顺序固定的多个数值特征，对应一个标量目标。时间序列调用方可将多个物理量的历史窗口展平成一个样本，将指定未来步的值作为目标；调用方负责时间排序、跳步切段和训练/验证划分。多步递推时，未来控制量由调用方提供。

模型在训练集上学习逐列均值与标准差，并对目标去均值，使用 `exp(-gamma * squared_distance)` 构造核矩阵，通过 Cholesky 分解求解 `(K + regularization * I) * weights = centered_targets`。参数必须有限且大于零。失败的训练保留已有模型，失败的预测保留输出值。并发只读预测可共享已训练实例，训练操作需由调用方同步。

## 构建与测试

```bat
cmake -S modules/machine_learning -B build-machine-learning -DAEROSIM_EIGEN_ROOT=<Eigen头文件目录>
cmake --build build-machine-learning --config Debug
cd build-machine-learning
ctest -C Debug --output-on-failure
```

工具链使用仓库规定的 VS2017 v141 x64 Debug；Eigen 也可通过标准 `Eigen3` CMake 包提供。独立构建通过后，顶层设置 `AEROSIM_BUILD_MACHINE_LEARNING=ON` 可构建该模块，调用方链接 `aerosim::machine_learning`。

测试使用 4 个合成样本验证非线性交互、一个留出查询和已知控制量下的三步递推，并检查无效输入、重复样本和重新训练。该测试用于数值及接口验收，实际求解数据的预测精度需要时间顺序回测。
