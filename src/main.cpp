#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm> // Для std::nth_element
#include <cmath>     // Для std::max
#include <stdexcept> // Для std::runtime_error

// --- Главный заголовок Open3D ---
#include "open3d/Open3D.h"
// ----------------------------------

/**
 * @struct Point3D
 * @brief Простая структура для хранения 3D-координат (X, Y, Z).
 */
struct Point3D {
    double x, y, z;
};

/**
 * @brief Находит перцентиль в векторе (модифицирует вектор на месте).
 * @param data Вектор данных (будет частично отсортирован).
 * @param p Перцентиль (от 0.0 до 1.0).
 * @return Значение в указанном перцентиле.
 */
double get_percentile(std::vector<double>& data, double p) {
    if (data.empty()) {
        throw std::runtime_error("Невозможно вычислить перцентиль для пустого вектора.");
    }
    size_t idx = static_cast<size_t>(p * (data.size() - 1));
    std::nth_element(data.begin(), data.begin() + idx, data.end());
    return data[idx];
}

/**
 * @brief Находит медиану в векторе (модифицирует вектор на месте).
 * @param data Вектор данных (будет частично отсортирован).
 * @return Медианное значение.
 */
double get_median(std::vector<double>& data) {
    if (data.empty()) {
        throw std::runtime_error("Невозможно вычислить медиану для пустого вектора.");
    }
    size_t n = data.size();
    std::nth_element(data.begin(), data.begin() + n / 2, data.end());
    
    // Для нечетного N, data[n/2] - это медиана.
    // Для четного N, это нижняя медиана, что достаточно для центрирования.
    return data[n / 2];
}


int main() {
    // -----------------------------------------------------------------
    // 1. ЗАГРУЗКА ДАННЫХ
    // -----------------------------------------------------------------
    
    std::string filename = "../optimized_points.txt";
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Ошибка: Не удалось открыть файл " << filename << std::endl;
        return 1;
    }

    std::cout << "Файл открыт. Считываю точки..." << std::endl;
    std::vector<Point3D> points_data;
    
    // Векторы для робастной статистики
    std::vector<double> x_coords, y_coords, z_coords;
    
    double x, y, z;
    while (file >> x >> y >> z) {
        points_data.push_back({x, y, z});
        // Собираем отдельные координаты
        x_coords.push_back(x);
        y_coords.push_back(y);
        z_coords.push_back(z);
    }
    file.close();

    std::cout << "  Всего загружено точек: " << points_data.size() << std::endl;
    if (points_data.empty()) {
        std::cerr << "Точки не загружены. Проверьте файл." << std::endl;
        return -1;
    }

    // -----------------------------------------------------------------
    // 2. ВЫЧИСЛЕНИЕ РОБАСТНЫХ ЦЕНТРА И МАСШТАБА
    // -----------------------------------------------------------------
    std::cout << "Вычисление робастных (медиана/перцентиль) границ..." << std::endl;

    // 2.1. Находим медианный центр
    double median_x = get_median(x_coords); // x_coords будет изменен!
    double median_y = get_median(y_coords); // y_coords будет изменен!
    double median_z = get_median(z_coords); // z_coords будет изменен!

    Eigen::Vector3d robust_center(median_x, median_y, median_z);
    std::cout << "  Робастный центр (медиана): (" << median_x << ", " << median_y << ", " << median_z << ")" << std::endl;

    // 2.2. Находим 5-й и 95-й перцентили (используем УЖЕ измененные векторы)
    double p5_x = get_percentile(x_coords, 0.05);
    double p95_x = get_percentile(x_coords, 0.95);
    
    double p5_y = get_percentile(y_coords, 0.05);
    double p95_y = get_percentile(y_coords, 0.95);

    double p5_z = get_percentile(z_coords, 0.05);
    double p95_z = get_percentile(z_coords, 0.95);

    // 2.3. Находим максимальный диапазон (игнорируя 10% выбросов)
    double extent_x = p95_x - p5_x;
    double extent_y = p95_y - p5_y;
    double extent_z = p95_z - p5_z;
    
    double max_robust_extent = std::max({extent_x, extent_y, extent_z});
    
    double scale = 1.0;
    if (max_robust_extent > 1e-6) { // Защита от деления на ноль
        scale = 1.0 / max_robust_extent;
    }

    std::cout << "  Робастный масштаб: " << scale << " (на основе 90% данных)" << std::endl;

    // -----------------------------------------------------------------
    // 3. СОЗДАНИЕ ОБЛАКА И НОРМАЛИЗАЦИЯ
    // -----------------------------------------------------------------
    std::cout << "Создание геометрии Open3D и применение нормализации..." << std::endl;
    auto cloud_points = std::make_shared<open3d::geometry::PointCloud>();

    // *Важно*: Мы добавляем ОРИГИНАЛЬНЫЕ точки в облако
    for (const auto& p : points_data) {
        cloud_points->points_.push_back(Eigen::Vector3d(p.x, p.y, p.z));
    }
    
    // ...а затем применяем к облаку робастный сдвиг и масштаб
    cloud_points->Translate(-robust_center);
    cloud_points->Scale(scale, Eigen::Vector3d(0.0, 0.0, 0.0));
    
    // Раскрасим точки
    cloud_points->PaintUniformColor(Eigen::Vector3d(0.9, 0.9, 0.1)); // Ярко-желтый

    // -----------------------------------------------------------------
    // 4. ВИЗУАЛИЗАЦИЯ (ПРОСТАЯ ВЕРСИЯ)
    // -----------------------------------------------------------------
    std::cout << "Отображение геометрии..." << std::endl;
    std::cout << "Нажмите 'Q' в окне для выхода." << std::endl;
    
    open3d::visualization::DrawGeometries({cloud_points});

    std::cout << "Окно визуализации закрыто." << std::endl;
    return 0;
}