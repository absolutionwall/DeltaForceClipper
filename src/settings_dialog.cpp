#include "settings_dialog.hpp"
#include "clip_library.hpp"
#include "plugin-main.hpp"

#include <QCheckBox>
#include <QAbstractItemView>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDialog>
#include <QEventLoop>
#include <QFile>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHash>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QString uiText(const QString &source, const QString &language)
{
    static const QHash<QString, QHash<QString, QString>> translations{
        {QStringLiteral("OBS status"), {{QStringLiteral("es"), QStringLiteral("Estado de OBS")}, {QStringLiteral("pt-BR"), QStringLiteral("Status do OBS")}}},
        {QStringLiteral("OBS:"), {{QStringLiteral("es"), QStringLiteral("OBS:")}, {QStringLiteral("pt-BR"), QStringLiteral("OBS:")}}},
        {QStringLiteral("Replay Buffer:"), {{QStringLiteral("es"), QStringLiteral("Buffer de repeticiÃ³n:")}, {QStringLiteral("pt-BR"), QStringLiteral("Buffer de replay:")}}},
        {QStringLiteral("Last clip:"), {{QStringLiteral("es"), QStringLiteral("Ãšltimo clip:")}, {QStringLiteral("pt-BR"), QStringLiteral("Ãšltimo clipe:")}}},
        {QStringLiteral("Live clipping"), {{QStringLiteral("es"), QStringLiteral("Clips automÃ¡ticos")}, {QStringLiteral("pt-BR"), QStringLiteral("Clipes automÃ¡ticos")}}},
        {QStringLiteral("Replay buffer duration:"), {{QStringLiteral("es"), QStringLiteral("DuraciÃ³n del buffer:")}, {QStringLiteral("pt-BR"), QStringLiteral("DuraÃ§Ã£o do buffer:")}}},
        {QStringLiteral("Clip library folder:"), {{QStringLiteral("es"), QStringLiteral("Carpeta de clips:")}, {QStringLiteral("pt-BR"), QStringLiteral("Pasta de clipes:")}}},
        {QStringLiteral("Automatically start Replay Buffer when OBS is ready"), {{QStringLiteral("es"), QStringLiteral("Iniciar el buffer automÃ¡ticamente al abrir OBS")}, {QStringLiteral("pt-BR"), QStringLiteral("Iniciar o buffer automaticamente quando o OBS estiver pronto")}}},
        {QStringLiteral("Killfeed detector"), {{QStringLiteral("es"), QStringLiteral("Detector del killfeed")}, {QStringLiteral("pt-BR"), QStringLiteral("Detector do killfeed")}}},
        {QStringLiteral("Enable killfeed detector (save clips on kills)"), {{QStringLiteral("es"), QStringLiteral("Activar detector (guardar clips al conseguir bajas)")}, {QStringLiteral("pt-BR"), QStringLiteral("Ativar detector (salvar clipes ao eliminar)")}}},
        {QStringLiteral("Detector events file:"), {{QStringLiteral("es"), QStringLiteral("Archivo de eventos:")}, {QStringLiteral("pt-BR"), QStringLiteral("Arquivo de eventos:")}}},
        {QStringLiteral("Application language:"), {{QStringLiteral("es"), QStringLiteral("Idioma de la aplicaciÃ³n:")}, {QStringLiteral("pt-BR"), QStringLiteral("Idioma do aplicativo:")}}},
        {QStringLiteral("Requested pre-event context:"), {{QStringLiteral("es"), QStringLiteral("Contexto previo solicitado:")}, {QStringLiteral("pt-BR"), QStringLiteral("Contexto anterior solicitado:")}}},
        {QStringLiteral("Grouping quiet time:"), {{QStringLiteral("es"), QStringLiteral("Tiempo de espera para agrupar:")}, {QStringLiteral("pt-BR"), QStringLiteral("Tempo de espera para agrupar:")}}},
        {QStringLiteral("Player label for event records:"), {{QStringLiteral("es"), QStringLiteral("Nombre del jugador en los eventos:")}, {QStringLiteral("pt-BR"), QStringLiteral("Nome do jogador nos eventos:")}}},
        {QStringLiteral("Detector:"), {{QStringLiteral("es"), QStringLiteral("Detector:")}, {QStringLiteral("pt-BR"), QStringLiteral("Detector:")}}},
        {QStringLiteral("Last detector event:"), {{QStringLiteral("es"), QStringLiteral("Ãšltimo evento detectado:")}, {QStringLiteral("pt-BR"), QStringLiteral("Ãšltimo evento detectado:")}}},
        {QStringLiteral("Configuration"), {{QStringLiteral("es"), QStringLiteral("ConfiguraciÃ³n")}, {QStringLiteral("pt-BR"), QStringLiteral("ConfiguraÃ§Ã£o")}}},
        {QStringLiteral("History"), {{QStringLiteral("es"), QStringLiteral("Historial")}, {QStringLiteral("pt-BR"), QStringLiteral("HistÃ³rico")}}},
        {QStringLiteral("Date"), {{QStringLiteral("es"), QStringLiteral("Fecha")}, {QStringLiteral("pt-BR"), QStringLiteral("Data")}}},
        {QStringLiteral("Event"), {{QStringLiteral("es"), QStringLiteral("Evento")}, {QStringLiteral("pt-BR"), QStringLiteral("Evento")}}},
        {QStringLiteral("Map"), {{QStringLiteral("es"), QStringLiteral("Mapa")}, {QStringLiteral("pt-BR"), QStringLiteral("Mapa")}}},
        {QStringLiteral("File"), {{QStringLiteral("es"), QStringLiteral("Archivo")}, {QStringLiteral("pt-BR"), QStringLiteral("Arquivo")}}},
        {QStringLiteral("Start Buffer"), {{QStringLiteral("es"), QStringLiteral("Iniciar buffer")}, {QStringLiteral("pt-BR"), QStringLiteral("Iniciar buffer")}}},
        {QStringLiteral("Stop Buffer"), {{QStringLiteral("es"), QStringLiteral("Detener buffer")}, {QStringLiteral("pt-BR"), QStringLiteral("Parar buffer")}}},
        {QStringLiteral("Test Save Clip"), {{QStringLiteral("es"), QStringLiteral("Probar guardado")}, {QStringLiteral("pt-BR"), QStringLiteral("Testar salvamento")}}},
        {QStringLiteral("Save"), {{QStringLiteral("es"), QStringLiteral("Guardar")}, {QStringLiteral("pt-BR"), QStringLiteral("Salvar")}}},
        {QStringLiteral("Close"), {{QStringLiteral("es"), QStringLiteral("Cerrar")}, {QStringLiteral("pt-BR"), QStringLiteral("Fechar")}}},
        {QStringLiteral("Ready"), {{QStringLiteral("es"), QStringLiteral("Listo")}, {QStringLiteral("pt-BR"), QStringLiteral("Pronto")}}},
        {QStringLiteral("Not ready"), {{QStringLiteral("es"), QStringLiteral("No disponible")}, {QStringLiteral("pt-BR"), QStringLiteral("IndisponÃ­vel")}}},
        {QStringLiteral("Active"), {{QStringLiteral("es"), QStringLiteral("Activo")}, {QStringLiteral("pt-BR"), QStringLiteral("Ativo")}}},
        {QStringLiteral("Off"), {{QStringLiteral("es"), QStringLiteral("Inactivo")}, {QStringLiteral("pt-BR"), QStringLiteral("Desativado")}}},
        {QStringLiteral("Running"), {{QStringLiteral("es"), QStringLiteral("En ejecuciÃ³n")}, {QStringLiteral("pt-BR"), QStringLiteral("Em execuÃ§Ã£o")}}},
        {QStringLiteral("Stopped"), {{QStringLiteral("es"), QStringLiteral("Detenido")}, {QStringLiteral("pt-BR"), QStringLiteral("Parado")}}},
        {QStringLiteral("None"), {{QStringLiteral("es"), QStringLiteral("Ninguno")}, {QStringLiteral("pt-BR"), QStringLiteral("Nenhum")}}},
        {QStringLiteral("No clips yet."), {{QStringLiteral("es"), QStringLiteral("AÃºn no hay clips.")}, {QStringLiteral("pt-BR"), QStringLiteral("Ainda nÃ£o hÃ¡ clipes.")}}},
        {QStringLiteral("Unable to load clip history: %1"), {{QStringLiteral("es"), QStringLiteral("No se pudo cargar el historial: %1")}, {QStringLiteral("pt-BR"), QStringLiteral("NÃ£o foi possÃ­vel carregar o histÃ³rico: %1")}}},
        {QStringLiteral("Unable to refresh clip history: %1"), {{QStringLiteral("es"), QStringLiteral("No se pudo actualizar el historial: %1")}, {QStringLiteral("pt-BR"), QStringLiteral("NÃ£o foi possÃ­vel atualizar o histÃ³rico: %1")}}},
        {QStringLiteral("Delta Force Clipper"), {{QStringLiteral("es"), QStringLiteral("Delta Force Clipper")}, {QStringLiteral("pt-BR"), QStringLiteral("Delta Force Clipper")}}},
        {QStringLiteral("Popup message triggers"), {{QStringLiteral("es"), QStringLiteral("Mensajes que activan clips")}, {QStringLiteral("pt-BR"), QStringLiteral("Mensagens que acionam clipes")}}},
        {QStringLiteral("Message phrase"), {{QStringLiteral("es"), QStringLiteral("Frase del mensaje")}, {QStringLiteral("pt-BR"), QStringLiteral("Frase da mensagem")}}},
        {QStringLiteral("Clip type"), {{QStringLiteral("es"), QStringLiteral("Tipo de clip")}, {QStringLiteral("pt-BR"), QStringLiteral("Tipo de clipe")}}},
        {QStringLiteral("Add trigger"), {{QStringLiteral("es"), QStringLiteral("AÃ±adir mensaje")}, {QStringLiteral("pt-BR"), QStringLiteral("Adicionar mensagem")}}},
        {QStringLiteral("Remove selected"), {{QStringLiteral("es"), QStringLiteral("Quitar seleccionado")}, {QStringLiteral("pt-BR"), QStringLiteral("Remover selecionado")}}},
        {QStringLiteral("Kill"), {{QStringLiteral("es"), QStringLiteral("Baja")}, {QStringLiteral("pt-BR"), QStringLiteral("Abate")}}},
        {QStringLiteral("Double Kill"), {{QStringLiteral("es"), QStringLiteral("Doble baja")}, {QStringLiteral("pt-BR"), QStringLiteral("Abate duplo")}}},
        {QStringLiteral("Triple Kill"), {{QStringLiteral("es"), QStringLiteral("Triple baja")}, {QStringLiteral("pt-BR"), QStringLiteral("Abate triplo")}}},
        {QStringLiteral("Multikill"), {{QStringLiteral("es"), QStringLiteral("Multibaja")}, {QStringLiteral("pt-BR"), QStringLiteral("MultiextermÃ­nio")}}},
        {QStringLiteral("Quadra Kill"), {{QStringLiteral("es"), QStringLiteral("CuÃ¡druple baja")}, {QStringLiteral("pt-BR"), QStringLiteral("Abate quÃ¡druplo")}}},
        {QStringLiteral("Penta Kill"), {{QStringLiteral("es"), QStringLiteral("QuÃ­ntuple baja")}, {QStringLiteral("pt-BR"), QStringLiteral("Abate quÃ­ntuplo")}}},
        {QStringLiteral("Clip tag"), {{QStringLiteral("es"), QStringLiteral("Etiqueta del clip")}, {QStringLiteral("pt-BR"), QStringLiteral("Etiqueta do clipe")}}},
        {QStringLiteral("Edit the phrase and optional tag by double-clicking. Choose a clip type, then Save to apply changes."), {{QStringLiteral("es"), QStringLiteral("Edita la frase y la etiqueta opcional con doble clic. Elige el tipo de clip y pulsa Guardar para aplicar los cambios.")}, {QStringLiteral("pt-BR"), QStringLiteral("Edite a frase e a etiqueta opcional com duplo clique. Escolha o tipo de clipe e clique em Salvar para aplicar.")}}},
        {QStringLiteral("Trigger phrases"), {{QStringLiteral("es"), QStringLiteral("Frases de activaciÃ³n")}, {QStringLiteral("pt-BR"), QStringLiteral("Frases de acionamento")}}},
        {QStringLiteral("Edit the phrase in the first column by double-clicking it. Select a clip type, then Save to apply changes."), {{QStringLiteral("es"), QStringLiteral("Edita la frase de la primera columna con doble clic. Elige el tipo de clip y pulsa Guardar para aplicar los cambios.")}, {QStringLiteral("pt-BR"), QStringLiteral("Edite a frase da primeira coluna com duplo clique. Escolha o tipo de clipe e clique em Salvar para aplicar.")}}},
        {QStringLiteral("OCR capture region"), {{QStringLiteral("es"), QStringLiteral("RegiÃ³n de captura OCR")}, {QStringLiteral("pt-BR"), QStringLiteral("RegiÃ£o de captura OCR")}}},
        {QStringLiteral("Default OCR regions are active."), {{QStringLiteral("es"), QStringLiteral("Se usan las regiones OCR predeterminadas.")}, {QStringLiteral("pt-BR"), QStringLiteral("As regiÃµes OCR padrÃ£o estÃ£o ativas.")}}},
        {QStringLiteral("A custom screen region is selected."), {{QStringLiteral("es"), QStringLiteral("Hay una regiÃ³n de pantalla personalizada seleccionada.")}, {QStringLiteral("pt-BR"), QStringLiteral("Uma regiÃ£o de tela personalizada estÃ¡ selecionada.")}}},
        {QStringLiteral("Select screen region"), {{QStringLiteral("es"), QStringLiteral("Seleccionar regiÃ³n de pantalla")}, {QStringLiteral("pt-BR"), QStringLiteral("Selecionar regiÃ£o da tela")}}},
        {QStringLiteral("Use default regions"), {{QStringLiteral("es"), QStringLiteral("Usar regiones predeterminadas")}, {QStringLiteral("pt-BR"), QStringLiteral("Usar regiÃµes padrÃ£o")}}},
        {QStringLiteral("Switch to the game. The primary screen will be captured in %1 seconds."), {{QStringLiteral("es"), QStringLiteral("Cambia al juego. La pantalla principal se capturarÃ¡ en %1 segundos.")}, {QStringLiteral("pt-BR"), QStringLiteral("Alterne para o jogo. A tela principal serÃ¡ capturada em %1 segundos.")}}},
        {QStringLiteral("Drag around the game notification text. The selected rectangle replaces the default OCR regions."), {{QStringLiteral("es"), QStringLiteral("Arrastra alrededor del texto de notificaciÃ³n del juego. El rectÃ¡ngulo seleccionado reemplaza las regiones OCR predeterminadas.")}, {QStringLiteral("pt-BR"), QStringLiteral("Arraste ao redor do texto de notificaÃ§Ã£o do jogo. O retÃ¢ngulo selecionado substitui as regiÃµes OCR padrÃ£o.")}}},
        {QStringLiteral("Select a rectangle at least 16 pixels wide and high."), {{QStringLiteral("es"), QStringLiteral("Selecciona un rectÃ¡ngulo de al menos 16 pÃ­xeles de ancho y alto.")}, {QStringLiteral("pt-BR"), QStringLiteral("Selecione um retÃ¢ngulo com pelo menos 16 pixels de largura e altura.")}}},
        {QStringLiteral("Screen capture unavailable"), {{QStringLiteral("es"), QStringLiteral("Captura de pantalla no disponible")}, {QStringLiteral("pt-BR"), QStringLiteral("Captura de tela indisponÃ­vel")}}},
        {QStringLiteral("Invalid OCR capture region"), {{QStringLiteral("es"), QStringLiteral("RegiÃ³n de captura OCR no vÃ¡lida")}, {QStringLiteral("pt-BR"), QStringLiteral("RegiÃ£o de captura OCR invÃ¡lida")}}},
        {QStringLiteral("The primary screen could not be captured."), {{QStringLiteral("es"), QStringLiteral("No se pudo capturar la pantalla principal.")}, {QStringLiteral("pt-BR"), QStringLiteral("NÃ£o foi possÃ­vel capturar a tela principal.")}}}
    };
    const auto sourceTranslations = translations.constFind(source);
    if (sourceTranslations == translations.cend()) {
        return source;
    }
    return sourceTranslations->value(language, source);
}

QString appText(const DeltaForceClipper *core, const QString &source)
{
    return uiText(source, core->uiLanguage());
}

class RegionPreviewWidget final : public QWidget {
public:
    RegionPreviewWidget(const QPixmap &screenshot, const QSize &maximumSize, QWidget *parent = nullptr)
        : QWidget(parent),
          screenshot_(screenshot.scaled(maximumSize, Qt::KeepAspectRatio, Qt::SmoothTransformation))
    {
        setFixedSize(screenshot_.size());
        setMouseTracking(true);
    }

    QRectF normalizedSelection() const
    {
        const QRect selection = selection_.normalized().intersected(rect());
        if (selection.width() < 16 || selection.height() < 16 || width() <= 0 || height() <= 0) {
            return {};
        }
        return QRectF(selection.x() / static_cast<qreal>(width()),
                      selection.y() / static_cast<qreal>(height()),
                      selection.width() / static_cast<qreal>(width()),
                      selection.height() / static_cast<qreal>(height()));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.drawPixmap(rect(), screenshot_);
        painter.fillRect(rect(), QColor(0, 0, 0, 105));
        if (!selection_.isEmpty()) {
            const QRect selection = selection_.normalized().intersected(rect());
            painter.drawPixmap(selection, screenshot_, selection);
            painter.setPen(QPen(QColor(80, 255, 140), 2));
            painter.drawRect(selection.adjusted(0, 0, -1, -1));
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton) {
            return;
        }
        dragging_ = true;
        selection_ = QRect(event->position().toPoint(), event->position().toPoint());
        update();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!dragging_) {
            return;
        }
        selection_.setBottomRight(event->position().toPoint());
        update();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (dragging_ && event->button() == Qt::LeftButton) {
            selection_.setBottomRight(event->position().toPoint());
            dragging_ = false;
            update();
        }
    }

private:
    QPixmap screenshot_;
    QRect selection_;
    bool dragging_ = false;
};

class RegionSelectionDialog final : public QDialog {
public:
    RegionSelectionDialog(const QPixmap &screenshot, const QSize &maximumSize,
                          const QString &language, QWidget *parent = nullptr)
        : QDialog(parent), language_(language)
    {
        setWindowTitle(uiText(QStringLiteral("Select OCR screen region"), language_));
        auto *layout = new QVBoxLayout(this);
        auto *instruction = new QLabel(
            uiText(QStringLiteral("Drag around the game notification text. The selected rectangle replaces the default OCR regions."),
                   language_), this);
        instruction->setWordWrap(true);
        layout->addWidget(instruction);

        preview_ = new RegionPreviewWidget(screenshot, maximumSize, this);
        layout->addWidget(preview_, 0, Qt::AlignCenter);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, [this] {
            if (preview_->normalizedSelection().isEmpty()) {
                QMessageBox::information(
                    this, uiText(QStringLiteral("Select OCR screen region"), language_),
                    uiText(QStringLiteral("Select a rectangle at least 16 pixels wide and high."),
                           language_));
                return;
            }
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        adjustSize();
    }

    QRectF normalizedSelection() const
    {
        return preview_->normalizedSelection();
    }

private:
    QString language_;
    RegionPreviewWidget *preview_ = nullptr;
};
}

SettingsDialog::SettingsDialog(DeltaForceClipper *core, QWidget *parent)
    : QDockWidget(parent), core_(core), contentWidget_(new QWidget(this))
{
    buildUi();
    setWidget(contentWidget_);
    refreshStatus();
    refreshHistory();
    connect(&refreshTimer_, &QTimer::timeout, this, &SettingsDialog::refreshStatus);
    refreshTimer_.start(1000);
    connect(&historyRefreshTimer_, &QTimer::timeout, this, [this] {
        if (historyTable_ && historyTable_->isVisible()) {
            refreshHistory();
        }
    });
    historyRefreshTimer_.start(5000);
}

void SettingsDialog::buildUi()
{
    ocrRegions_ = core_->ocrRegions();
    setWindowTitle(appText(core_, QStringLiteral("Delta Force Clipper")));
    setMinimumWidth(560);

    auto *layout = qobject_cast<QVBoxLayout *>(contentWidget_->layout());
    if (!layout) {
        layout = new QVBoxLayout(contentWidget_);
    }
    auto *tabs = new QTabWidget(contentWidget_);
    layout->addWidget(tabs);

    auto *configTab = new QScrollArea(tabs);
    configTab->setWidgetResizable(true);
    auto *configContent = new QWidget(configTab);
    auto *configLayout = new QVBoxLayout(configContent);

    auto *statusBox = new QGroupBox(appText(core_, QStringLiteral("OBS status")), configContent);
    auto *statusLayout = new QFormLayout(statusBox);
    obsStatus_ = new QLabel(this);
    replayStatus_ = new QLabel(this);
    detectorStatus_ = new QLabel(this);
    lastClip_ = new QLabel(appText(core_, QStringLiteral("None")), this);
    lastClip_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusLayout->addRow(appText(core_, QStringLiteral("OBS:")), obsStatus_);
    statusLayout->addRow(appText(core_, QStringLiteral("Replay Buffer:")), replayStatus_);
    statusLayout->addRow(appText(core_, QStringLiteral("Detector:")), detectorStatus_);
    statusLayout->addRow(appText(core_, QStringLiteral("Last clip:")), lastClip_);
    configLayout->addWidget(statusBox);

    auto *settingsBox = new QGroupBox(appText(core_, QStringLiteral("Live clipping")), configContent);
    auto *settingsLayout = new QFormLayout(settingsBox);
    bufferSeconds_ = new QSpinBox(this);
    bufferSeconds_->setRange(5, 180);
    bufferSeconds_->setSuffix(QStringLiteral(" s"));
    settingsLayout->addRow(appText(core_, QStringLiteral("Replay buffer duration:")), bufferSeconds_);
    clipFolder_ = new QLineEdit(this);
    settingsLayout->addRow(appText(core_, QStringLiteral("Clip library folder:")), clipFolder_);
    autoStart_ = new QCheckBox(appText(core_, QStringLiteral("Automatically start Replay Buffer when OBS is ready")), this);
    settingsLayout->addRow(QString(), autoStart_);
    configLayout->addWidget(settingsBox);

    auto *detectorBox = new QGroupBox(appText(core_, QStringLiteral("Killfeed detector")), configContent);
    auto *detectorLayout = new QFormLayout(detectorBox);
    detectorEnabled_ = new QCheckBox(appText(core_, QStringLiteral("Enable killfeed detector (save clips on kills)")), this);
    detectorLayout->addRow(detectorEnabled_);
    eventsFile_ = new QLineEdit(this);
    detectorLayout->addRow(appText(core_, QStringLiteral("Detector events file:")), eventsFile_);

    uiLanguage_ = new QComboBox(this);
    uiLanguage_->addItem(QStringLiteral("English"), QStringLiteral("en"));
    uiLanguage_->addItem(QStringLiteral("EspaÃ±ol"), QStringLiteral("es"));
    uiLanguage_->addItem(QStringLiteral("PortuguÃªs (Brasil)"), QStringLiteral("pt-BR"));
    uiLanguage_->setCurrentIndex(qMax(0, uiLanguage_->findData(core_->uiLanguage())));
    detectorLayout->addRow(appText(core_, QStringLiteral("Application language:")), uiLanguage_);

    preTime_ = new QSpinBox(this);
    preTime_->setRange(0, 30000);
    preTime_->setSuffix(QStringLiteral(" ms"));
    detectorLayout->addRow(appText(core_, QStringLiteral("Requested pre-event context:")), preTime_);
    postTime_ = new QSpinBox(this);
    postTime_->setRange(0, 10000);
    postTime_->setSuffix(QStringLiteral(" ms"));
    detectorLayout->addRow(appText(core_, QStringLiteral("Grouping quiet time:")), postTime_);
    playerNick_ = new QLineEdit(this);
    detectorLayout->addRow(appText(core_, QStringLiteral("Player label for event records:")), playerNick_);

    detectorStatus_->setObjectName(QStringLiteral("detectorRuntimeStatus"));
    auto *lastEventValue = new QLabel(this);
    lastEventValue->setObjectName(QStringLiteral("detectorLastEvent"));
    detectorLayout->addRow(appText(core_, QStringLiteral("Last detector event:")), lastEventValue);
    configLayout->addWidget(detectorBox);

    auto *ocrRegionBox = new QGroupBox(appText(core_, QStringLiteral("OCR capture region")), configContent);
    auto *ocrRegionLayout = new QVBoxLayout(ocrRegionBox);
    ocrRegionStatus_ = new QLabel(ocrRegionBox);
    ocrRegionLayout->addWidget(ocrRegionStatus_);
    auto *ocrRegionActions = new QHBoxLayout();
    auto *selectRegion = new QPushButton(appText(core_, QStringLiteral("Select screen region")), ocrRegionBox);
    auto *defaultRegions = new QPushButton(appText(core_, QStringLiteral("Use default regions")), ocrRegionBox);
    ocrRegionActions->addWidget(selectRegion);
    ocrRegionActions->addWidget(defaultRegions);
    ocrRegionLayout->addLayout(ocrRegionActions);
    configLayout->addWidget(ocrRegionBox);
    connect(selectRegion, &QPushButton::clicked, this, &SettingsDialog::selectOcrRegion);
    connect(defaultRegions, &QPushButton::clicked, this, [this] {
        ocrRegions_ = core_->defaultOcrRegions();
        updateOcrRegionStatus();
    });
    updateOcrRegionStatus();

    configTab->setWidget(configContent);
    tabs->addTab(configTab, appText(core_, QStringLiteral("Configuration")));

    auto *triggerTab = new QWidget(tabs);
    auto *triggerLayout = new QVBoxLayout(triggerTab);
    auto *triggerHelp = new QLabel(appText(
        core_, QStringLiteral("Edit the phrase and optional tag by double-clicking. Choose a clip type, then Save to apply changes.")),
        triggerTab);
    triggerHelp->setWordWrap(true);
    triggerLayout->addWidget(triggerHelp);

    triggerMessagesTable_ = new QTableWidget(triggerTab);
    triggerMessagesTable_->setColumnCount(3);
    triggerMessagesTable_->setHorizontalHeaderLabels({
        appText(core_, QStringLiteral("Message phrase")),
        appText(core_, QStringLiteral("Clip type")),
        appText(core_, QStringLiteral("Clip tag"))});
    triggerMessagesTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    triggerMessagesTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    triggerMessagesTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    triggerMessagesTable_->setColumnWidth(2, 120);
    triggerMessagesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    triggerMessagesTable_->setEditTriggers(QAbstractItemView::DoubleClicked |
                                           QAbstractItemView::EditKeyPressed |
                                           QAbstractItemView::SelectedClicked);
    triggerMessagesTable_->setAlternatingRowColors(true);
    triggerMessagesTable_->setToolTip(appText(
        core_, QStringLiteral("Edit the phrase and optional tag by double-clicking. Choose a clip type, then Save to apply changes.")));
    triggerLayout->addWidget(triggerMessagesTable_);
    for (const QJsonValue &value : core_->triggerMessages()) {
        const QJsonObject message = value.toObject();
        addTriggerMessageRow(message.value(QStringLiteral("phrase")).toString(),
                             message.value(QStringLiteral("event")).toString(),
                             message.value(QStringLiteral("tag")).toString());
    }

    auto *triggerActions = new QHBoxLayout();
    auto *addTrigger = new QPushButton(appText(core_, QStringLiteral("Add trigger")), triggerTab);
    auto *removeTrigger = new QPushButton(appText(core_, QStringLiteral("Remove selected")), triggerTab);
    triggerActions->addWidget(addTrigger);
    triggerActions->addWidget(removeTrigger);
    triggerActions->addStretch();
    triggerLayout->addLayout(triggerActions);
    connect(addTrigger, &QPushButton::clicked, this, [this] {
        const int row = triggerMessagesTable_->rowCount();
        addTriggerMessageRow(QString(), QStringLiteral("multikill"), QString());
        triggerMessagesTable_->selectRow(row);
        triggerMessagesTable_->editItem(triggerMessagesTable_->item(row, 0));
    });
    connect(removeTrigger, &QPushButton::clicked, this, [this] {
        const int row = triggerMessagesTable_->currentRow();
        if (row >= 0) {
            triggerMessagesTable_->removeRow(row);
        }
    });
    tabs->addTab(triggerTab, appText(core_, QStringLiteral("Trigger phrases")));

    auto *historyTab = new QWidget(tabs);
    auto *historyLayout = new QVBoxLayout(historyTab);
    historyStatus_ = new QLabel(historyTab);
    historyLayout->addWidget(historyStatus_);
    historyTable_ = new QTableWidget(historyTab);
    historyTable_->setColumnCount(4);
    historyTable_->setHorizontalHeaderLabels({
        appText(core_, QStringLiteral("Date")), appText(core_, QStringLiteral("Event")),
        appText(core_, QStringLiteral("Map")), appText(core_, QStringLiteral("File"))});
    historyTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    historyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyLayout->addWidget(historyTable_);
    tabs->addTab(historyTab, appText(core_, QStringLiteral("History")));
    connect(tabs, &QTabWidget::currentChanged, this, [this, tabs, historyTab](int index) {
        if (tabs->widget(index) == historyTab) {
            refreshHistory();
        }
    });

    auto *actions = new QHBoxLayout();
    auto *start = new QPushButton(appText(core_, QStringLiteral("Start Buffer")), contentWidget_);
    auto *stop = new QPushButton(appText(core_, QStringLiteral("Stop Buffer")), contentWidget_);
    auto *test = new QPushButton(appText(core_, QStringLiteral("Test Save Clip")), contentWidget_);
    actions->addWidget(start);
    actions->addWidget(stop);
    actions->addStretch();
    actions->addWidget(test);
    layout->addLayout(actions);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close, contentWidget_);
    buttons->button(QDialogButtonBox::Save)->setText(appText(core_, QStringLiteral("Save")));
    buttons->button(QDialogButtonBox::Close)->setText(appText(core_, QStringLiteral("Close")));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::saveSettings);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDockWidget::close);
    connect(test, &QPushButton::clicked, this, &SettingsDialog::saveTestClip);
    connect(start, &QPushButton::clicked, this, &SettingsDialog::startReplayBuffer);
    connect(stop, &QPushButton::clicked, this, &SettingsDialog::stopReplayBuffer);

    bufferSeconds_->setValue(core_->bufferSeconds());
    clipFolder_->setText(core_->clipFolder());
    autoStart_->setChecked(core_->autoStartReplayBuffer());
    detectorEnabled_->setChecked(core_->detectorEnabled());
    eventsFile_->setText(core_->eventsFile());
    preTime_->setValue(core_->preTime());
    postTime_->setValue(core_->postTime());
    playerNick_->setText(core_->playerNick());
}

void SettingsDialog::addTriggerMessageRow(const QString &phrase, const QString &eventType, const QString &tag)
{
    const int row = triggerMessagesTable_->rowCount();
    triggerMessagesTable_->insertRow(row);
    triggerMessagesTable_->setItem(row, 0, new QTableWidgetItem(phrase));
    triggerMessagesTable_->setItem(row, 2, new QTableWidgetItem(tag));

    auto *type = new QComboBox(triggerMessagesTable_);
    type->addItem(appText(core_, QStringLiteral("Kill")), QStringLiteral("kill"));
    type->addItem(appText(core_, QStringLiteral("Double Kill")), QStringLiteral("double_kill"));
    type->addItem(appText(core_, QStringLiteral("Triple Kill")), QStringLiteral("triple_kill"));
    type->addItem(appText(core_, QStringLiteral("Multikill")), QStringLiteral("multikill"));
    type->addItem(appText(core_, QStringLiteral("Quadra Kill")), QStringLiteral("quadra_kill"));
    type->addItem(appText(core_, QStringLiteral("Penta Kill")), QStringLiteral("penta_kill"));
    const int typeIndex = type->findData(eventType);
    type->setCurrentIndex(typeIndex >= 0 ? typeIndex : type->findData(QStringLiteral("multikill")));
    triggerMessagesTable_->setCellWidget(row, 1, type);
}

void SettingsDialog::clearUi()
{
    QLayout *layout = contentWidget_->layout();
    if (!layout) {
        return;
    }
    const auto clear = [](QLayout *parentLayout, const auto &clearLayout) -> void {
        while (QLayoutItem *item = parentLayout->takeAt(0)) {
            if (QLayout *childLayout = item->layout()) {
                clearLayout(childLayout, clearLayout);
            } else if (QWidget *widget = item->widget()) {
                delete widget;
            }
            delete item;
        }
    };
    clear(layout, clear);
}

void SettingsDialog::refreshStatus()
{
    obsStatus_->setText(appText(core_, core_->obsReady() ? QStringLiteral("Ready") : QStringLiteral("Not ready")));
    replayStatus_->setText(appText(core_, core_->replayBufferActive() ? QStringLiteral("Active") : QStringLiteral("Off")));
    detectorStatus_->setText(appText(core_, core_->detectorRunning() ? QStringLiteral("Running") : QStringLiteral("Stopped")));
    lastClip_->setText(core_->lastClipPath().isEmpty() ? appText(core_, QStringLiteral("None")) : core_->lastClipPath());

    QLabel *lastEvent = findChild<QLabel *>(QStringLiteral("detectorLastEvent"));
    if (lastEvent) {
        lastEvent->setText(core_->lastEvent().isEmpty() ? appText(core_, QStringLiteral("None")) : core_->lastEvent());
    }
}

void SettingsDialog::refreshHistory()
{
    if (!historyTable_) {
        return;
    }
    historyTable_->setRowCount(0);
    ClipLibrary library;
    QString pruneError;
    if (!library.pruneMissingFiles(&pruneError)) {
        historyStatus_->setText(appText(core_, QStringLiteral("Unable to refresh clip history: %1")).arg(pruneError));
        return;
    }
    QFile file(library.libraryPath());
    if (!file.exists()) {
        historyStatus_->setText(appText(core_, QStringLiteral("No clips yet.")));
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        historyStatus_->setText(appText(core_, QStringLiteral("Unable to load clip history: %1")).arg(file.errorString()));
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        historyStatus_->setText(appText(core_, QStringLiteral("Unable to load clip history: %1")).arg(parseError.errorString()));
        return;
    }
    const QJsonArray clips = doc.object().value(QStringLiteral("clips")).toArray();
    historyStatus_->setText(clips.isEmpty() ? appText(core_, QStringLiteral("No clips yet.")) : QString());
    historyTable_->setRowCount(clips.size());
    for (int row = 0; row < clips.size(); ++row) {
        const QJsonObject clip = clips.at(clips.size() - row - 1).toObject();
        historyTable_->setItem(row, 0, new QTableWidgetItem(clip.value(QStringLiteral("created_at")).toString()));
        historyTable_->setItem(row, 1, new QTableWidgetItem(clip.value(QStringLiteral("event")).toString()));
        historyTable_->setItem(row, 2, new QTableWidgetItem(clip.value(QStringLiteral("map")).toString()));
        historyTable_->setItem(row, 3, new QTableWidgetItem(clip.value(QStringLiteral("file")).toString()));
    }
}

void SettingsDialog::selectOcrRegion()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        QMessageBox::warning(this, appText(core_, QStringLiteral("Screen capture unavailable")),
                             appText(core_, QStringLiteral("The primary screen could not be captured.")));
        return;
    }

    QDialog countdown(this);
    countdown.setWindowTitle(appText(core_, QStringLiteral("OCR capture region")));
    countdown.setWindowModality(Qt::NonModal);
    auto *countdownLayout = new QVBoxLayout(&countdown);
    auto *countdownText = new QLabel(countdownLayout->parentWidget());
    countdownText->setWordWrap(true);
    countdownLayout->addWidget(countdownText);
    auto *cancel = new QDialogButtonBox(QDialogButtonBox::Cancel, &countdown);
    countdownLayout->addWidget(cancel);
    connect(cancel, &QDialogButtonBox::rejected, &countdown, &QDialog::reject);

    int secondsRemaining = 5;
    QPixmap screenshot;
    bool captureComplete = false;
    QEventLoop countdownLoop;
    const QString countdownMessage = QStringLiteral("Switch to the game. The primary screen will be captured in %1 seconds.");
    countdownText->setText(appText(core_, countdownMessage).arg(secondsRemaining));
    QTimer countdownTimer;
    connect(&countdownTimer, &QTimer::timeout, &countdown, [&] {
        --secondsRemaining;
        if (secondsRemaining <= 0) {
            countdownTimer.stop();
            countdown.hide();
            QTimer::singleShot(100, &countdown, [&] {
                screenshot = screen->grabWindow(0);
                captureComplete = true;
                countdownLoop.quit();
            });
        } else {
            countdownText->setText(appText(core_, countdownMessage).arg(secondsRemaining));
        }
    });
    connect(&countdown, &QDialog::finished, &countdownLoop, &QEventLoop::quit);
    countdown.show();
    countdownTimer.start(1000);
    countdownLoop.exec();
    countdownTimer.stop();
    if (!captureComplete) {
        return;
    }

    if (screenshot.isNull()) {
        QMessageBox::warning(this, appText(core_, QStringLiteral("Screen capture unavailable")),
                             appText(core_, QStringLiteral("The primary screen could not be captured.")));
        return;
    }

    QSize maximumSize = screen->availableGeometry().size() - QSize(40, 120);
    if (maximumSize.width() <= 0 || maximumSize.height() <= 0) {
        maximumSize = screen->size();
    }
    RegionSelectionDialog selection(screenshot, maximumSize, core_->uiLanguage(), this);
    if (selection.exec() != QDialog::Accepted) {
        return;
    }

    const QRectF normalized = selection.normalizedSelection();
    ocrRegions_ = QJsonArray{QJsonObject{
        {QStringLiteral("x"), normalized.x()},
        {QStringLiteral("y"), normalized.y()},
        {QStringLiteral("width"), normalized.width()},
        {QStringLiteral("height"), normalized.height()}
    }};
    updateOcrRegionStatus();
}

void SettingsDialog::updateOcrRegionStatus()
{
    if (!ocrRegionStatus_) {
        return;
    }
    const QString status = ocrRegions_ == core_->defaultOcrRegions()
                               ? QStringLiteral("Default OCR regions are active.")
                               : QStringLiteral("A custom screen region is selected.");
    ocrRegionStatus_->setText(appText(core_, status));
}

void SettingsDialog::saveSettings()
{
    QString regionError;
    if (!core_->setOcrRegions(ocrRegions_, &regionError)) {
        QMessageBox::warning(this, appText(core_, QStringLiteral("Invalid OCR capture region")), regionError);
        return;
    }
    const QString previousLanguage = core_->uiLanguage();
    core_->setBufferSeconds(bufferSeconds_->value());
    core_->setClipFolder(clipFolder_->text());
    core_->setAutoStartReplayBuffer(autoStart_->isChecked());
    core_->setDetectorEnabled(detectorEnabled_->isChecked());
    core_->setEventsFile(eventsFile_->text());
    core_->setUiLanguage(uiLanguage_->currentData().toString());
    core_->setPreTime(preTime_->value());
    core_->setPostTime(postTime_->value());
    core_->setPlayerNick(playerNick_->text());
    QJsonArray triggerMessages;
    for (int row = 0; row < triggerMessagesTable_->rowCount(); ++row) {
        const QTableWidgetItem *phrase = triggerMessagesTable_->item(row, 0);
        auto *type = qobject_cast<QComboBox *>(triggerMessagesTable_->cellWidget(row, 1));
        if (!phrase || !type || phrase->text().trimmed().isEmpty()) {
            continue;
        }
        const QTableWidgetItem *tag = triggerMessagesTable_->item(row, 2);
        triggerMessages.append(QJsonObject{
            {QStringLiteral("phrase"), phrase->text().trimmed()},
            {QStringLiteral("event"), type->currentData().toString()},
            {QStringLiteral("tag"), tag ? tag->text().trimmed() : QString()}
        });
    }
    core_->setTriggerMessages(triggerMessages);
    core_->saveConfig();
    core_->applyReplayBufferDuration();
    core_->applyDetectorSettings();
    if (previousLanguage != core_->uiLanguage()) {
        clearUi();
        buildUi();
    }
    refreshStatus();
    refreshHistory();
}

void SettingsDialog::saveTestClip()
{
    core_->saveReplayBufferAs(QStringLiteral("manual_test"));
    refreshStatus();
}

void SettingsDialog::startReplayBuffer()
{
    core_->startReplayBuffer();
    refreshStatus();
}

void SettingsDialog::stopReplayBuffer()
{
    core_->stopReplayBuffer();
    refreshStatus();
}
