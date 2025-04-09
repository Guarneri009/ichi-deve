#![deny(unused_imports)]

use std::{io::ErrorKind, path::PathBuf};

use clap::{Parser, Subcommand};
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
    render::{RenderPlugin, builder::RendererBuilder, settings::WgpuSettings},
    style::Style,
};
use maplibre_winit::{WinitEnvironment, WinitMapWindowConfig};

#[cfg(feature = "headless")]
mod headless;

#[derive(Parser)]
#[clap(author, version, about, long_about = None)]
#[clap(propagate_version = true)]
struct Cli {
    #[clap(subcommand)]
    command: Commands,
}

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

        let mut map = Map::new(
            Style::default(),
            kernel,
            renderer_builder,
            vec![
                Box::new(RenderPlugin::default()),
                Box::new(maplibre::vector::VectorPlugin::<
                    maplibre::vector::DefaultVectorTransferables,
                >::default()),
                // Box::new(maplibre::raster::RasterPlugin::<
                //     maplibre::raster::DefaultRasterTransferables,
                // >::default()),
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
