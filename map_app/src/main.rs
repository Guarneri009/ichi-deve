use clap::{Parser, Subcommand};
use csscolorparser::Color;
use maplibre::{
    coords::LatLon,
    environment::OffscreenKernelConfig,
    event_loop::EventLoop,
    io::apc::SchedulerAsyncProcedureCall,
    kernel::{Kernel, KernelBuilder},
    map::Map,
    platform::{
        ReqwestOffscreenKernelEnvironment, http_client::ReqwestHttpClient, run_multithreaded,
        scheduler::TokioScheduler,
    },
    raster::DefaultRasterTransferables,
    render::{RenderPlugin, builder::RendererBuilder, settings::WgpuSettings},
    style::{
        Style,
        layer::{FillPaint, LayerPaint, StyleLayer},
        raster::RasterLayer,
        source::{Source, VectorSource},
    },
    vector::DefaultVectorTransferables,
};
use maplibre_winit::{WinitEnvironment, WinitMapWindowConfig};
use std::{collections::HashMap, str::FromStr};
use std::{io::ErrorKind, path::PathBuf};

#[cfg(feature = "headless")]
mod headless;

#[derive(Parser)]
#[clap(author, version, about, long_about = None)]
#[clap(propagate_version = true)]
struct Cli {
    #[clap(subcommand)]
    command: Commands,
}

// type RasterTransfer = maplibre::raster::DefaultRasterTransferables;
// type VecterTransfer = maplibre::raster::DefaultRasterTransferables;

fn parse_lat_long(env: &str) -> Result<LatLon, std::io::Error> {
    let split = env.split(',').collect::<Vec<_>>();
    if let (Some(latitude), Some(longitude)) = (split.first(), split.get(1)) {
        Ok(LatLon::new(
            latitude.parse::<f64>().unwrap(),
            longitude.parse::<f64>().unwrap(),
        ))
    } else {
        Err(std::io::Error::new(
            ErrorKind::InvalidData,
            "Failed to parse latitude and longitude.",
        ))
    }
}

#[derive(Subcommand)]
enum Commands {
    Headed {},
    #[cfg(feature = "headless")]
    Headless {
        #[clap(default_value_t = 400)]
        tile_size: u32,
        #[clap(
            value_parser = clap::builder::ValueParser::new(parse_lat_long),
            default_value_t = LatLon::new(48.0345697188, 11.3475219363)
        )]
        min: LatLon,
        #[clap(
            value_parser = clap::builder::ValueParser::new(parse_lat_long),
            default_value_t = LatLon::new(48.255861, 11.7917815798)
        )]
        max: LatLon,
    },
}

fn create_gsi_vector_source() -> Source {
    // Step 1: VectorSource インスタンスを作成
    let vector_data = VectorSource {
        // tiles フィールドは Option<String> なので、単一の URL 文字列を Some でラップ
        tiles: Some("https://cyberjapandata.gsi.go.jp/xyz/experimental_bvmap/{z}/{x}/{y}.pbf".to_string()),
        minzoom: Some(4),
        maxzoom: Some(16),
        attribution: Some("<a href='https://maps.gsi.go.jp/development/vt.html' target='_blank'>国土地理院ベクトルタイル</a>".to_string()),
        // bounds や scheme は指定しない場合は None
        bounds: None,
        scheme: None, // None か Some(TileAddressingScheme::XYZ) を指定
    };

    // Step 2: Source::Vector バリアントでラップして返す
    Source::Vector(vector_data)
}

fn create_gsi_raster_source() -> Source {
    // Step 1: VectorSource インスタンスを作成 (Rasterでも同じ構造体を使う定義になっている)
    let raster_data = VectorSource {
        // tiles フィールドは Option<String> なので、単一の URL 文字列を Some でラップ
        tiles: Some("https://cyberjapandata.gsi.go.jp/xyz/std/{z}/{x}/{y}.png".to_string()),
        maxzoom: Some(18),
        attribution: Some("<a href='https://maps.gsi.go.jp/development/ichiran.html' target='_blank'>地理院タイル</a>".to_string()),
        // 他のフィールドは None
        bounds: None,
        minzoom: None,
        scheme: None,
    };

    // Step 2: Source::Raster バリアントでラップして返す
    Source::Raster(raster_data)
}

pub fn run_headed_map_custom<P>(
    cache_path: Option<P>,
    window_config: WinitMapWindowConfig<()>,
    wgpu_settings: WgpuSettings,
) where
    P: Into<PathBuf>,
{
    run_multithreaded(async {
        type Environment<S, HC, APC> =
            WinitEnvironment<S, HC, ReqwestOffscreenKernelEnvironment, APC, ()>;

        let cache_path = cache_path.map(|path| path.into());
        let client = ReqwestHttpClient::new(cache_path.clone());

        let kernel: Kernel<Environment<_, _, _>> = KernelBuilder::new()
            .with_map_window_config(window_config)
            .with_http_client(client.clone())
            .with_apc(SchedulerAsyncProcedureCall::new(
                TokioScheduler::new(),
                OffscreenKernelConfig {
                    cache_directory: cache_path.map(|path| path.to_str().unwrap().to_string()),
                },
            ))
            .with_scheduler(TokioScheduler::new())
            .build();

        let renderer_builder = RendererBuilder::new().with_wgpu_settings(wgpu_settings);

        // language=JSON
        let style_json_str = r##"
{
    "version": 8,
    "name": "Test Style",
    "metadata": {},
    "sources": {
    "openmaptiles": {
        "type": "vector",
        "url": "https://maps.tuerantuer.org/europe_germany/tiles.json"
    }
    },
    "layers": [
    {
        "id": "background",
        "type": "background",
        "paint": {"background-color": "rgb(239,239,239)"}
    },
    {
        "id": "transportation",
        "type": "line",
        "source": "openmaptiles",
        "source-layer": "transportation",
        "paint": {
        "line-color": "#3D3D3D"
        }
    },
    {
        "id": "boundary",
        "type": "line",
        "source": "openmaptiles",
        "source-layer": "boundary",
        "paint": {
        "line-color": "#3D3D3D"
        }
    },
    {
        "id": "building",
        "minzoom": 14,
        "maxzoom": 15,
        "type": "fill",
        "source": "openmaptiles",
        "source-layer": "building",
        "paint": {
        "line-color": "#3D3D3D"
        }
    }
    ]
}
"##;
        let style_test: Style = serde_json::from_str(style_json_str).unwrap();

        let gsi_vector = create_gsi_vector_source();
        let gsi_raster = create_gsi_raster_source();
        println!("GSI Vector Source: {:?}", gsi_vector);
        println!("GSI Raster Source: {:?}", gsi_raster);

        // HashMap に入れる例
        let mut custom_source = HashMap::new();
        custom_source.insert("gsi-vector".to_string(), gsi_vector);
        custom_source.insert("gsi-raster".to_string(), gsi_raster);

        // Style 型の変数を作成
        let custom_map_style: Style = Style {
            version: 8,
            name: "Default Style".to_string(),
            metadata: Default::default(),
            sources: custom_source,
            center: Some([35.681, 139.767]), // 東京駅付近の座標 [緯度, 経度]
            zoom: Some(13.0),
            pitch: Some(0.0),
            // layers: vec![StyleLayer {
            //     index: 0,
            //     id: "ls-boundary-cty".to_string(),
            //     // id: "ls-coastline".to_string(),
            //     maxzoom: None,
            //     minzoom: None,
            //     metadata: None,
            //     paint: Some(LayerPaint::Fill(FillPaint {
            //         fill_color: Some(Color::from_str("#00dfdf").unwrap()),
            //     })),
            //     //source: Some("gsi-raster".to_string()),
            //     source: None,
            //     source_layer: Some("ls-boundary-cty".to_string()),
            //     //source_layer: None,
            // }],
            layers: vec![
                StyleLayer {
                    index: 0,
                    id: "park".to_string(),
                    maxzoom: None,
                    minzoom: None,
                    metadata: None,
                    paint: Some(LayerPaint::Fill(FillPaint {
                        fill_color: Some(Color::from_str("#c8facc").unwrap()),
                    })),
                    source: None,
                    source_layer: Some("park".to_string()),
                },
                StyleLayer {
                    index: 8,
                    id: "raster-gsi".to_string(),
                    maxzoom: None,
                    minzoom: None,
                    metadata: None,
                    paint: Some(LayerPaint::Raster(RasterLayer::default())),
                    source: None,
                    //source_layer: Some("raster".to_string()),
                    source_layer: Some("gsi-raster".to_string()),
                },
            ],
        };

        let map_style_center_tokyo_defult = Style {
            center: Some([35.681, 139.767]), // 東京駅付近の座標 [緯度, 経度]
            ..Style::default()
        };

        let mut map = Map::new(
            //style_test,
            //Style::default(),
            map_style_center_tokyo_defult,
            //custom_map_style,
            //
            kernel,
            renderer_builder,
            vec![
                Box::new(RenderPlugin::default()),
                // ベクター
                Box::new(maplibre::vector::VectorPlugin::<DefaultVectorTransferables>::default()),
                // ラスタ
                Box::new(maplibre::raster::RasterPlugin::<DefaultRasterTransferables>::default()),
                #[cfg(debug_assertions)]
                Box::new(maplibre::debug::DebugPlugin::default()),
            ],
        )
        .unwrap();

        #[cfg(not(target_os = "android"))]
        {
            map.initialize_renderer().await.unwrap();
        }

        map.window_mut()
            .take_event_loop()
            .expect("event loop is not available")
            .run(map, None)
            .expect("event loop creation failed")
    })
}

fn main() {
    env_logger::init_from_env(env_logger::Env::default().default_filter_or("info"));

    #[cfg(feature = "trace")]
    maplibre::platform::trace::enable_tracing();

    let cli = Cli::parse();

    // You can check for the existence of subcommands, and if found use their
    // matches just as you would the top level cmd
    match &cli.command {
        Commands::Headed {} => run_headed_map_custom(
            Some(PathBuf::from("./maplibre-cache".to_string())),
            WinitMapWindowConfig::new("maplibre".to_string()),
            WgpuSettings {
                backends: Some(maplibre::render::settings::Backends::all()),
                ..WgpuSettings::default()
            },
        ),
        #[cfg(feature = "headless")]
        Commands::Headless {
            tile_size,
            min,
            max,
        } => {
            maplibre::platform::run_multithreaded(async {
                headless::run_headless(*tile_size, *min, *max).await
            });
        }
    }
}
