#include <gtest/gtest.h>
#include <qapplication.h>

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    QApplication a(argc, argv);
    return RUN_ALL_TESTS();
}
