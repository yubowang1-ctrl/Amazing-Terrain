#pragma once

#include <QMainWindow>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include "realtime.h"
#include "utils/aspectratiowidget/aspectratiowidget.hpp"

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    void initialize();
    void finish();

private:
    void connectUIElements();
    void connectParam1();
    void connectParam2();
    void connectParam3();
    void connectParam4();
    void connectParam5();
    void connectParam6();
    void connectNear();
    void connectFar();

    // From old Project 6
    // void connectPerPixelFilter();
    // void connectKernelBasedFilter();

    void connectUploadFile();
    void connectSaveImage();
    void connectExtraCredit();

    void connectColorGrade();

    Realtime *realtime;
    AspectRatioWidget *aspectRatioWidget;

    // From old Project 6
    // QCheckBox *filter1;
    // QCheckBox *filter2;

    QPushButton *uploadFile;
    QPushButton *saveImage;
    QSlider *p1Slider;
    QSlider *p2Slider;
    QSlider *p3Slider;
    QSlider *p4Slider;
    QSlider *p5Slider;
    QSlider *p6Slider;
    QSpinBox *p1Box;
    QSpinBox *p2Box;
    QSpinBox *p3Box;
    QSpinBox *p4Box;
    QSpinBox *p5Box;
    QSpinBox *p6Box;
    QSlider *nearSlider;
    QSlider *farSlider;
    QDoubleSpinBox *nearBox;
    QDoubleSpinBox *farBox;

    // Extra Credit:
    QCheckBox *ec1;
    QCheckBox *ec2;
    QCheckBox *ec3;
    QCheckBox *ec4;

    QCheckBox *checkBoxColdBlue = nullptr;
    QCheckBox *checkBoxRainy    = nullptr;

private slots:
    // From old Project 6
    // void onPerPixelFilter();
    // void onKernelBasedFilter();

    void onUploadFile();
    void onSaveImage();
    void onValChangeP1(int newValue);
    void onValChangeP2(int newValue);
    void onValChangeP3(int newValue);
    void onValChangeP4(int newValue);
    void onValChangeP5(int newValue);
    void onValChangeP6(int newValue);
    void onValChangeNearSlider(int newValue);
    void onValChangeFarSlider(int newValue);
    void onValChangeNearBox(double newValue);
    void onValChangeFarBox(double newValue);

    // Extra Credit:
    void onExtraCredit1();
    void onExtraCredit2();
    void onExtraCredit3();
    void onExtraCredit4();

    void on_checkBoxColdBlue_toggled(bool checked);
    void on_checkBoxRainy_toggled(bool checked);
};
