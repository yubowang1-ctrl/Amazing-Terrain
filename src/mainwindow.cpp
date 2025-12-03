#include "mainwindow.h"
#include "settings.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QSettings>
#include <QLabel>
#include <QGroupBox>
#include <iostream>

void MainWindow::initialize() {
    realtime = new Realtime;
    aspectRatioWidget = new AspectRatioWidget(this);
    aspectRatioWidget->setAspectWidget(realtime, 3.f/4.f);
    QHBoxLayout *hLayout = new QHBoxLayout; // horizontal alignment
    QVBoxLayout *vLayout = new QVBoxLayout(); // vertical alignment
    vLayout->setAlignment(Qt::AlignTop);
    hLayout->addLayout(vLayout);
    hLayout->addWidget(aspectRatioWidget, 1);
    this->setLayout(hLayout);

    // Create labels in sidebox
    QFont font;
    font.setPointSize(12);
    font.setBold(true);
    QLabel *tesselation_label = new QLabel(); // Parameters label
    tesselation_label->setText("Terrain generation");
    tesselation_label->setFont(font);
    QLabel *camera_label = new QLabel(); // Camera label
    camera_label->setText("Camera");
    camera_label->setFont(font);

    // From old Project 6
    // QLabel *filters_label = new QLabel(); // Filters label
    // filters_label->setText("Filters");
    // filters_label->setFont(font);

    QLabel *ec_label = new QLabel(); // Extra Credit label
    ec_label->setText("Extra Credit");
    ec_label->setFont(font);
    QLabel *param1_label = new QLabel(); // Parameter 1 label
    param1_label->setText("Frequency:");
    QLabel *param2_label = new QLabel(); // Parameter 2 label
    param2_label->setText("Height:");
    QLabel *param3_label = new QLabel(); // new added
    param3_label->setText("Terrain distortion & river curvature (EC3 on): ");
    QLabel *param4_label = new QLabel(); // new added
    param4_label->setText("Vegetation Clusters (EC4 on):");
    QLabel *param5_label = new QLabel();
    param5_label->setText("Trees per cluster (EC4 on):");
    QLabel *param6_label = new QLabel();
    param6_label->setText("Leaf density (EC4 on):");
    QLabel *near_label = new QLabel(); // Near plane label
    near_label->setText("Near Plane:");
    QLabel *far_label = new QLabel(); // Far plane label
    far_label->setText("Far Plane:");

    // color grading UI
    QLabel *grade_label = new QLabel();
    grade_label->setText("Color Grading");
    grade_label->setFont(font);

    checkBoxColdBlue = new QCheckBox();
    checkBoxColdBlue->setText(QStringLiteral("Cold Blue (Snowy)"));
    checkBoxColdBlue->setChecked(settings.colorGradePreset == 1);

    checkBoxRainy = new QCheckBox();
    checkBoxRainy->setText(QStringLiteral("Rainy / Overcast"));
    checkBoxRainy->setChecked(settings.colorGradePreset == 2);


    // From old Project 6
    // // Create checkbox for per-pixel filter
    // filter1 = new QCheckBox();
    // filter1->setText(QStringLiteral("Per-Pixel Filter"));
    // filter1->setChecked(false);
    // // Create checkbox for kernel-based filter
    // filter2 = new QCheckBox();
    // filter2->setText(QStringLiteral("Kernel-Based Filter"));
    // filter2->setChecked(false);

    // Create file uploader for scene file
    uploadFile = new QPushButton();
    uploadFile->setText(QStringLiteral("Upload Scene File"));
    
    saveImage = new QPushButton();
    saveImage->setText(QStringLiteral("Save Image"));

    // Creates the boxes containing the parameter sliders and number boxes
    QGroupBox *p1Layout = new QGroupBox(); // horizonal slider 1 alignment
    QHBoxLayout *l1 = new QHBoxLayout();
    QGroupBox *p2Layout = new QGroupBox(); // horizonal slider 2 alignment
    QHBoxLayout *l2 = new QHBoxLayout();
    QGroupBox *p3Layout = new QGroupBox(); // new added
    QHBoxLayout *l3 = new QHBoxLayout();
    QGroupBox *p4Layout = new QGroupBox(); // new added
    QHBoxLayout *l4 = new QHBoxLayout();
    QGroupBox *p5Layout = new QGroupBox(); // trees per cluster
    QHBoxLayout *l5 = new QHBoxLayout();
    QGroupBox *p6Layout = new QGroupBox(); // leaf density
    QHBoxLayout *l6 = new QHBoxLayout();

    // Create slider controls to control parameters
    p1Slider = new QSlider(Qt::Orientation::Horizontal); // Parameter 1 slider
    p1Slider->setTickInterval(1);
    p1Slider->setMinimum(1);
    p1Slider->setMaximum(25);
    p1Slider->setValue(1);

    p1Box = new QSpinBox();
    p1Box->setMinimum(1);
    p1Box->setMaximum(25);
    p1Box->setSingleStep(1);
    p1Box->setValue(1);

    p2Slider = new QSlider(Qt::Orientation::Horizontal); // Parameter 2 slider
    p2Slider->setTickInterval(1);
    p2Slider->setMinimum(1);
    p2Slider->setMaximum(25);
    p2Slider->setValue(1);

    p2Box = new QSpinBox();
    p2Box->setMinimum(1);
    p2Box->setMaximum(25);
    p2Box->setSingleStep(1);
    p2Box->setValue(1);

    // === Terrain distortion slider ===
    p3Slider = new QSlider(Qt::Orientation::Horizontal);
    p3Slider->setTickInterval(1);
    p3Slider->setMinimum(1);
    p3Slider->setMaximum(25);
    p3Slider->setValue(1);

    p3Box = new QSpinBox();
    p3Box->setMinimum(1);
    p3Box->setMaximum(25);
    p3Box->setSingleStep(1);
    p3Box->setValue(1);

    // === Vegetation coverage slider ===
    p4Slider = new QSlider(Qt::Horizontal);
    p4Slider->setTickInterval(1);
    p4Slider->setMinimum(1);
    p4Slider->setMaximum(100);
    p4Slider->setValue(25);

    p4Box = new QSpinBox();
    p4Box->setMinimum(1);
    p4Box->setMaximum(100);
    p4Box->setSingleStep(1);
    p4Box->setValue(25);

    // === Trees per cluster ===
    p5Slider = new QSlider(Qt::Horizontal);
    p5Slider->setTickInterval(1);
    p5Slider->setMinimum(1);
    p5Slider->setMaximum(30);
    p5Slider->setValue(12);

    p5Box = new QSpinBox();
    p5Box->setMinimum(1);
    p5Box->setMaximum(30);
    p5Box->setSingleStep(1);
    p5Box->setValue(12);

    // === Leaf density ===
    p6Slider = new QSlider(Qt::Horizontal);
    p6Slider->setTickInterval(1);
    p6Slider->setMinimum(1);
    p6Slider->setMaximum(15);
    p6Slider->setValue(12);

    p6Box = new QSpinBox();
    p6Box->setMinimum(1);
    p6Box->setMaximum(15);
    p6Box->setSingleStep(1);
    p6Box->setValue(12);

    // Adds the slider and number box to the parameter layouts
    l1->addWidget(p1Slider);
    l1->addWidget(p1Box);
    p1Layout->setLayout(l1);

    l2->addWidget(p2Slider);
    l2->addWidget(p2Box);
    p2Layout->setLayout(l2);

    l3->addWidget(p3Slider);
    l3->addWidget(p3Box);
    p3Layout->setLayout(l3);

    l4->addWidget(p4Slider);
    l4->addWidget(p4Box);
    p4Layout->setLayout(l4);

    l5->addWidget(p5Slider);
    l5->addWidget(p5Box);
    p5Layout->setLayout(l5);

    l6->addWidget(p6Slider);
    l6->addWidget(p6Box);
    p6Layout->setLayout(l6);

    // Creates the boxes containing the camera sliders and number boxes
    QGroupBox *nearLayout = new QGroupBox(); // horizonal near slider alignment
    QHBoxLayout *lnear = new QHBoxLayout();
    QGroupBox *farLayout = new QGroupBox(); // horizonal far slider alignment
    QHBoxLayout *lfar = new QHBoxLayout();

    // Create slider controls to control near/far planes
    nearSlider = new QSlider(Qt::Orientation::Horizontal); // Near plane slider
    nearSlider->setTickInterval(1);
    nearSlider->setMinimum(1);
    nearSlider->setMaximum(1000);
    nearSlider->setValue(10);

    nearBox = new QDoubleSpinBox();
    nearBox->setMinimum(0.01f);
    nearBox->setMaximum(10.f);
    nearBox->setSingleStep(0.1f);
    nearBox->setValue(0.1f);

    farSlider = new QSlider(Qt::Orientation::Horizontal); // Far plane slider
    farSlider->setTickInterval(1);
    farSlider->setMinimum(1000);
    farSlider->setMaximum(10000);
    farSlider->setValue(10000);

    farBox = new QDoubleSpinBox();
    farBox->setMinimum(10.f);
    farBox->setMaximum(100.f);
    farBox->setSingleStep(0.1f);
    farBox->setValue(100.f);

    // Adds the slider and number box to the parameter layouts
    lnear->addWidget(nearSlider);
    lnear->addWidget(nearBox);
    nearLayout->setLayout(lnear);

    lfar->addWidget(farSlider);
    lfar->addWidget(farBox);
    farLayout->setLayout(lfar);

    // Extra Credit:
    ec1 = new QCheckBox();
    ec1->setText(QStringLiteral("Extra Credit 1"));
    ec1->setChecked(false);

    ec2 = new QCheckBox();
    ec2->setText(QStringLiteral("Extra Credit 2"));
    ec2->setChecked(false);

    ec3 = new QCheckBox();
    ec3->setText(QStringLiteral("Extra Credit 3"));
    ec3->setChecked(false);

    ec4 = new QCheckBox();
    ec4->setText(QStringLiteral("Extra Credit 4"));
    ec4->setChecked(false);

    vLayout->addWidget(uploadFile);
    vLayout->addWidget(saveImage);
    vLayout->addWidget(tesselation_label);
    vLayout->addWidget(param1_label);
    vLayout->addWidget(p1Layout);
    vLayout->addWidget(param2_label);
    vLayout->addWidget(p2Layout);
    vLayout->addWidget(param3_label);
    vLayout->addWidget(p3Layout);
    vLayout->addWidget(param4_label);
    vLayout->addWidget(p4Layout);
    vLayout->addWidget(param5_label);
    vLayout->addWidget(p5Layout);
    vLayout->addWidget(param6_label);
    vLayout->addWidget(p6Layout);
    vLayout->addWidget(camera_label);
    vLayout->addWidget(near_label);
    vLayout->addWidget(nearLayout);
    vLayout->addWidget(far_label);
    vLayout->addWidget(farLayout);
    vLayout->addWidget(camera_label);
    vLayout->addWidget(near_label);
    vLayout->addWidget(nearLayout);
    vLayout->addWidget(far_label);
    vLayout->addWidget(farLayout);

    // From old Project 6
    // vLayout->addWidget(filters_label);
    // vLayout->addWidget(filter1);
    // vLayout->addWidget(filter2);

    // Extra Credit:
    vLayout->addWidget(ec_label);
    vLayout->addWidget(ec1);
    vLayout->addWidget(ec2);
    vLayout->addWidget(ec3);
    vLayout->addWidget(ec4);

    vLayout->addWidget(grade_label);
    vLayout->addWidget(checkBoxColdBlue);
    vLayout->addWidget(checkBoxRainy);

    connectUIElements();

    // Set default values of 5 for tesselation parameters
    onValChangeP1(5);
    onValChangeP2(5);

    // Set default Terrain Size / Vegetation
    onValChangeP3(1);
    onValChangeP4(1);
    onValChangeP5(1);
    onValChangeP6(1);

    // Set default values for near and far planes
    onValChangeNearBox(0.1f);
    onValChangeFarBox(10.f);
}

void MainWindow::finish() {
    realtime->finish();
    delete(realtime);
}

void MainWindow::connectUIElements() {
    // From old Project 6
    //connectPerPixelFilter();
    //connectKernelBasedFilter();
    connectUploadFile();
    connectSaveImage();
    connectParam1();
    connectParam2();
    connectParam3();
    connectParam4();
    connectParam5();
    connectParam6();
    connectNear();
    connectFar();
    connectExtraCredit();
    connectColorGrade();
}


// From old Project 6
// void MainWindow::connectPerPixelFilter() {
//     connect(filter1, &QCheckBox::clicked, this, &MainWindow::onPerPixelFilter);
// }
// void MainWindow::connectKernelBasedFilter() {
//     connect(filter2, &QCheckBox::clicked, this, &MainWindow::onKernelBasedFilter);
// }

void MainWindow::connectUploadFile() {
    connect(uploadFile, &QPushButton::clicked, this, &MainWindow::onUploadFile);
}

void MainWindow::connectSaveImage() {
    connect(saveImage, &QPushButton::clicked, this, &MainWindow::onSaveImage);
}

void MainWindow::connectParam1() {
    connect(p1Slider, &QSlider::valueChanged, this, &MainWindow::onValChangeP1);
    connect(p1Box, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &MainWindow::onValChangeP1);
}

void MainWindow::connectParam2() {
    connect(p2Slider, &QSlider::valueChanged, this, &MainWindow::onValChangeP2);
    connect(p2Box, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &MainWindow::onValChangeP2);
}

void MainWindow::connectParam3() {
    connect(p3Slider, &QSlider::valueChanged,
            this, &MainWindow::onValChangeP3);
    connect(p3Box, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &MainWindow::onValChangeP3);
}

void MainWindow::connectParam4() {
    connect(p4Slider, &QSlider::valueChanged,
            this, &MainWindow::onValChangeP4);
    connect(p4Box, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &MainWindow::onValChangeP4);
}

void MainWindow::connectParam5() {
    connect(p5Slider, &QSlider::valueChanged,
            this, &MainWindow::onValChangeP5);
    connect(p5Box, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &MainWindow::onValChangeP5);
}

void MainWindow::connectParam6() {
    connect(p6Slider, &QSlider::valueChanged,
            this, &MainWindow::onValChangeP6);
    connect(p6Box, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &MainWindow::onValChangeP6);
}

void MainWindow::connectNear() {
    connect(nearSlider, &QSlider::valueChanged, this, &MainWindow::onValChangeNearSlider);
    connect(nearBox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onValChangeNearBox);
}

void MainWindow::connectFar() {
    connect(farSlider, &QSlider::valueChanged, this, &MainWindow::onValChangeFarSlider);
    connect(farBox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onValChangeFarBox);
}

void MainWindow::connectExtraCredit() {
    connect(ec1, &QCheckBox::clicked, this, &MainWindow::onExtraCredit1);
    connect(ec2, &QCheckBox::clicked, this, &MainWindow::onExtraCredit2);
    connect(ec3, &QCheckBox::clicked, this, &MainWindow::onExtraCredit3);
    connect(ec4, &QCheckBox::clicked, this, &MainWindow::onExtraCredit4);
}

void MainWindow::connectColorGrade() {
    if (checkBoxColdBlue) {
        connect(checkBoxColdBlue, &QCheckBox::toggled,
                this, &MainWindow::on_checkBoxColdBlue_toggled);
    }
    if (checkBoxRainy) {
        connect(checkBoxRainy, &QCheckBox::toggled,
                this, &MainWindow::on_checkBoxRainy_toggled);
    }
}

// From old Project 6
// void MainWindow::onPerPixelFilter() {
//     settings.perPixelFilter = !settings.perPixelFilter;
//     realtime->settingsChanged();
// }
// void MainWindow::onKernelBasedFilter() {
//     settings.kernelBasedFilter = !settings.kernelBasedFilter;
//     realtime->settingsChanged();
// }

void MainWindow::onUploadFile() {
    // Get abs path of scene file
    QString configFilePath = QFileDialog::getOpenFileName(this, tr("Upload File"),
                                                          QDir::currentPath()
                                                              .append(QDir::separator())
                                                              .append("scenefiles")
                                                              .append(QDir::separator())
                                                              .append("realtime")
                                                              .append(QDir::separator())
                                                              .append("required"), tr("Scene Files (*.json)"));
    if (configFilePath.isNull()) {
        std::cout << "Failed to load null scenefile." << std::endl;
        return;
    }

    settings.sceneFilePath = configFilePath.toStdString();

    std::cout << "Loaded scenefile: \"" << configFilePath.toStdString() << "\"." << std::endl;

    realtime->sceneChanged();
}

void MainWindow::onSaveImage() {
    if (settings.sceneFilePath.empty()) {
        std::cout << "No scene file loaded." << std::endl;
        return;
    }
    std::string sceneName = settings.sceneFilePath.substr(0, settings.sceneFilePath.find_last_of("."));
    sceneName = sceneName.substr(sceneName.find_last_of("/")+1);
    QString filePath = QFileDialog::getSaveFileName(this, tr("Save Image"),
                                                    QDir::currentPath()
                                                        .append(QDir::separator())
                                                        .append("student_outputs")
                                                        .append(QDir::separator())
                                                        .append("realtime")
                                                        .append(QDir::separator())
                                                        .append("required")
                                                        .append(QDir::separator())
                                                        .append(sceneName), tr("Image Files (*.png)"));
    std::cout << "Saving image to: \"" << filePath.toStdString() << "\"." << std::endl;
    realtime->saveViewportImage(filePath.toStdString());
}

void MainWindow::onValChangeP1(int newValue) {
    p1Slider->setValue(newValue);
    p1Box->setValue(newValue);
    settings.shapeParameter1 = p1Slider->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeP2(int newValue) {
    p2Slider->setValue(newValue);
    p2Box->setValue(newValue);
    settings.shapeParameter2 = p2Slider->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeP3(int newValue) {
    p3Slider->setValue(newValue);
    p3Box->setValue(newValue);
    settings.shapeParameter3 = p3Slider->value();   // tile number
    realtime->settingsChanged();
}

void MainWindow::onValChangeP4(int newValue) {
    p4Slider->setValue(newValue);
    p4Box->setValue(newValue);
    settings.shapeParameter4 = p4Slider->value();   // tile number
    realtime->settingsChanged();
}

void MainWindow::onValChangeP5(int newValue) {
    p5Slider->setValue(newValue);
    p5Box->setValue(newValue);
    settings.shapeParameter5 = p5Slider->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeP6(int newValue) {
    p6Slider->setValue(newValue);
    p6Box->setValue(newValue);
    settings.shapeParameter6 = p6Slider->value();
    realtime->settingsChanged();
}


void MainWindow::onValChangeNearSlider(int newValue) {
    //nearSlider->setValue(newValue);
    nearBox->setValue(newValue/100.f);
    settings.nearPlane = nearBox->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeFarSlider(int newValue) {
    //farSlider->setValue(newValue);
    farBox->setValue(newValue/100.f);
    settings.farPlane = farBox->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeNearBox(double newValue) {
    nearSlider->setValue(int(newValue*100.f));
    //nearBox->setValue(newValue);
    settings.nearPlane = nearBox->value();
    realtime->settingsChanged();
}

void MainWindow::onValChangeFarBox(double newValue) {
    farSlider->setValue(int(newValue*100.f));
    //farBox->setValue(newValue);
    settings.farPlane = farBox->value();
    realtime->settingsChanged();
}

// Extra Credit:

void MainWindow::onExtraCredit1() {
    settings.extraCredit1 = !settings.extraCredit1;
    realtime->settingsChanged();
}

void MainWindow::onExtraCredit2() {
    settings.extraCredit2 = !settings.extraCredit2;
    realtime->settingsChanged();
}

void MainWindow::onExtraCredit3() {
    settings.extraCredit3 = !settings.extraCredit3;
    realtime->settingsChanged();
}

void MainWindow::onExtraCredit4() {
    settings.extraCredit4 = !settings.extraCredit4;
    realtime->settingsChanged();
}

void MainWindow::on_checkBoxColdBlue_toggled(bool checked)
{
    if (checked) {
        if (checkBoxRainy) {
            checkBoxRainy->setChecked(false);
        }
        settings.colorGradePreset = 1;
    } else {
        if (!checkBoxRainy || !checkBoxRainy->isChecked()) {
            settings.colorGradePreset = 0;
        }
    }

    realtime->update();
}

void MainWindow::on_checkBoxRainy_toggled(bool checked)
{
    if (checked) {
        if (checkBoxColdBlue) {
            checkBoxColdBlue->setChecked(false);
        }
        settings.colorGradePreset = 3;
    } else {
        if (!checkBoxColdBlue || !checkBoxColdBlue->isChecked()) {
            settings.colorGradePreset = 0;
        }
    }

    realtime->update();
}



