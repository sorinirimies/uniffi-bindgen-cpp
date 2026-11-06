//! A tour of the UniFFI features the C++ bindings have to support.

use std::collections::HashMap;
use std::sync::{Arc, Mutex};
use std::time::{Duration, SystemTime};

uniffi::setup_scaffolding!();

// ---- primitives -----------------------------------------------------------

#[uniffi::export]
pub fn add_u8(a: u8, b: u8) -> u8 {
    a.wrapping_add(b)
}
#[uniffi::export]
pub fn add_i32(a: i32, b: i32) -> i32 {
    a + b
}
#[uniffi::export]
pub fn add_u64(a: u64, b: u64) -> u64 {
    a + b
}
#[uniffi::export]
pub fn negate_i64(a: i64) -> i64 {
    -a
}
#[uniffi::export]
pub fn half_f32(a: f32) -> f32 {
    a / 2.0
}
#[uniffi::export]
pub fn half_f64(a: f64) -> f64 {
    a / 2.0
}
#[uniffi::export]
pub fn invert(flag: bool) -> bool {
    !flag
}
#[uniffi::export]
pub fn shout(text: String) -> String {
    text.to_uppercase()
}
#[uniffi::export]
pub fn reverse_bytes(data: Vec<u8>) -> Vec<u8> {
    data.into_iter().rev().collect()
}
#[uniffi::export]
pub fn nothing() {}
#[uniffi::export(default(greeting = "Hello"))]
pub fn greet(name: String, greeting: String) -> String {
    format!("{greeting}, {name}!")
}

// ---- containers -----------------------------------------------------------

#[uniffi::export]
pub fn maybe_double(v: Option<i32>) -> Option<i32> {
    v.map(|v| v * 2)
}
#[uniffi::export]
pub fn sum(values: Vec<i64>) -> i64 {
    values.iter().sum()
}
#[uniffi::export]
pub fn word_lengths(words: Vec<String>) -> HashMap<String, u32> {
    words.into_iter().map(|w| (w.clone(), w.len() as u32)).collect()
}
#[uniffi::export]
pub fn nested(v: Vec<Option<Vec<String>>>) -> Vec<Option<Vec<String>>> {
    v
}
#[uniffi::export]
pub fn timestamp_plus(t: SystemTime, d: Duration) -> SystemTime {
    t + d
}
#[uniffi::export]
pub fn double_duration(d: Duration) -> Duration {
    d * 2
}

// ---- records & enums ------------------------------------------------------

#[derive(Debug, Clone, PartialEq, uniffi::Record)]
pub struct Point {
    pub x: f64,
    pub y: f64,
}

#[derive(Debug, Clone, PartialEq, uniffi::Record)]
pub struct Settings {
    pub name: String,
    #[uniffi(default = 8)]
    pub retries: u32,
    #[uniffi(default = None)]
    pub label: Option<String>,
    #[uniffi(default = [])]
    pub tags: Vec<String>,
    pub origin: Point,
}

#[derive(Debug, Clone, Copy, PartialEq, uniffi::Enum)]
pub enum Color {
    Red,
    Green,
    Blue,
}

#[derive(Debug, Clone, PartialEq, uniffi::Enum)]
pub enum Shape {
    Circle { center: Point, radius: f64 },
    Rect(Point, Point),
    Empty,
}

#[uniffi::export]
pub fn mix(c: Color) -> Color {
    match c {
        Color::Red => Color::Green,
        Color::Green => Color::Blue,
        Color::Blue => Color::Red,
    }
}
#[uniffi::export]
pub fn area(s: Shape) -> f64 {
    match s {
        Shape::Circle { radius, .. } => 3.0 * radius * radius,
        Shape::Rect(a, b) => ((b.x - a.x) * (b.y - a.y)).abs(),
        Shape::Empty => 0.0,
    }
}
#[uniffi::export]
pub fn make_shape(kind: u8) -> Shape {
    match kind {
        0 => Shape::Circle { center: Point { x: 1.0, y: 2.0 }, radius: 2.0 },
        1 => Shape::Rect(Point { x: 0.0, y: 0.0 }, Point { x: 2.0, y: 3.0 }),
        _ => Shape::Empty,
    }
}
#[uniffi::export]
pub fn describe(s: Settings) -> String {
    format!("{}:{}:{:?}:{}:{}", s.name, s.retries, s.label, s.tags.join("+"), s.origin.x)
}

// ---- errors ---------------------------------------------------------------

#[derive(Debug, thiserror::Error, uniffi::Error)]
pub enum FlatError {
    #[error("not found")]
    NotFound,
    #[error("denied: {0}")]
    Denied(String),
}

#[derive(Debug, thiserror::Error, uniffi::Error)]
pub enum DataError {
    #[error("bad code {code}")]
    BadCode { code: i32 },
    #[error("too long: {0}")]
    TooLong(u32),
    #[error("plain")]
    Plain,
}

#[uniffi::export]
pub fn fail_flat(which: u8) -> Result<u8, FlatError> {
    match which {
        0 => Ok(7),
        1 => Err(FlatError::NotFound),
        _ => Err(FlatError::Denied("nope".into())),
    }
}
#[uniffi::export]
pub fn fail_data(which: u8) -> Result<(), DataError> {
    match which {
        0 => Ok(()),
        1 => Err(DataError::BadCode { code: 42 }),
        2 => Err(DataError::TooLong(9)),
        _ => Err(DataError::Plain),
    }
}
#[uniffi::export]
pub fn panics() {
    panic!("boom");
}

// ---- objects --------------------------------------------------------------

#[derive(uniffi::Object)]
#[uniffi::export(Display, Eq, Hash)]
pub struct Counter {
    value: Mutex<i64>,
}

impl std::fmt::Display for Counter {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Counter({})", self.value.lock().unwrap())
    }
}
impl PartialEq for Counter {
    fn eq(&self, other: &Self) -> bool {
        *self.value.lock().unwrap() == *other.value.lock().unwrap()
    }
}
impl Eq for Counter {}
impl std::hash::Hash for Counter {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.value.lock().unwrap().hash(state)
    }
}

#[uniffi::export]
impl Counter {
    #[uniffi::constructor]
    pub fn new(start: i64) -> Arc<Self> {
        Arc::new(Self { value: Mutex::new(start) })
    }
    #[uniffi::constructor]
    pub fn zero() -> Arc<Self> {
        Self::new(0)
    }
    #[uniffi::constructor]
    pub fn checked(start: i64) -> Result<Arc<Self>, FlatError> {
        if start < 0 {
            Err(FlatError::Denied("negative".into()))
        } else {
            Ok(Self::new(start))
        }
    }
    pub fn add(&self, n: i64) -> i64 {
        let mut v = self.value.lock().unwrap();
        *v += n;
        *v
    }
    pub fn get(&self) -> i64 {
        *self.value.lock().unwrap()
    }
    pub fn merge(&self, other: Arc<Counter>) {
        let n = other.get();
        self.add(n);
    }
    pub fn twin(&self) -> Arc<Counter> {
        Counter::new(self.get())
    }
    pub fn maybe_twin(&self, yes: bool) -> Option<Arc<Counter>> {
        yes.then(|| self.twin())
    }
    pub fn try_sub(&self, n: i64) -> Result<i64, DataError> {
        let mut v = self.value.lock().unwrap();
        if n > *v {
            Err(DataError::BadCode { code: n as i32 })
        } else {
            *v -= n;
            Ok(*v)
        }
    }
}

// ---- callback interfaces --------------------------------------------------

#[uniffi::export(callback_interface)]
pub trait Listener: Send + Sync {
    fn on_event(&self, name: String, value: i32) -> i32;
    fn on_done(&self);
    fn fallible(&self, n: i32) -> Result<String, FlatError>;
}

#[uniffi::export]
pub fn notify(listener: Box<dyn Listener>, n: i32) -> i32 {
    let mut total = 0;
    for i in 0..n {
        total += listener.on_event(format!("event-{i}"), i);
    }
    listener.on_done();
    total
}

#[uniffi::export]
pub fn try_listener(listener: Box<dyn Listener>, n: i32) -> Result<String, FlatError> {
    listener.fallible(n)
}

// ---- trait objects ---------------------------------------------------------

#[uniffi::export(with_foreign)]
pub trait Greeter: Send + Sync {
    fn greet(&self, name: String) -> String;
}

struct RustGreeter;
impl Greeter for RustGreeter {
    fn greet(&self, name: String) -> String {
        format!("rust says hi to {name}")
    }
}

#[uniffi::export]
pub fn rust_greeter() -> Arc<dyn Greeter> {
    Arc::new(RustGreeter)
}
#[uniffi::export]
pub fn use_greeter(g: Arc<dyn Greeter>, name: String) -> String {
    g.greet(name)
}

// ---- async ----------------------------------------------------------------

#[uniffi::export(async_runtime = "tokio")]
pub async fn slow_add(a: i32, b: i32) -> i32 {
    tokio::time::sleep(Duration::from_millis(50)).await;
    a + b
}
#[uniffi::export(async_runtime = "tokio")]
pub async fn slow_fail(fail: bool) -> Result<String, DataError> {
    tokio::time::sleep(Duration::from_millis(20)).await;
    if fail {
        Err(DataError::Plain)
    } else {
        Ok("fine".into())
    }
}
#[uniffi::export(async_runtime = "tokio")]
pub async fn slow_unit() {
    tokio::time::sleep(Duration::from_millis(10)).await;
}

#[derive(uniffi::Object)]
pub struct Worker {
    name: String,
}

#[uniffi::export(async_runtime = "tokio")]
impl Worker {
    #[uniffi::constructor]
    pub fn new(name: String) -> Arc<Self> {
        Arc::new(Self { name })
    }
    pub async fn work(&self, n: u32) -> String {
        tokio::time::sleep(Duration::from_millis(10)).await;
        format!("{} did {n}", self.name)
    }
}

#[uniffi::export(callback_interface)]
#[async_trait::async_trait]
pub trait AsyncSource: Send + Sync {
    async fn fetch(&self, key: String) -> String;
}

// ---- custom types ---------------------------------------------------------

pub struct Handle(pub u64);

uniffi::custom_newtype!(Handle, u64);

#[uniffi::export]
pub fn next_handle(h: Handle) -> Handle {
    Handle(h.0 + 1)
}

#[derive(Debug, Clone)]
pub struct Url(pub String);

uniffi::custom_newtype!(Url, String);

#[uniffi::export]
pub fn host_of(u: Url) -> String {
    u.0.trim_start_matches("https://").split('/').next().unwrap_or("").to_owned()
}

// ---- more ------------------------------------------------------------------

#[derive(Debug, thiserror::Error, uniffi::Error)]
#[uniffi(flat_error)]
pub enum MessageError {
    #[error("io failure: {0}")]
    Io(String),
    #[error("closed")]
    Closed,
}

#[uniffi::export]
pub fn fail_message(which: u8) -> Result<(), MessageError> {
    match which {
        0 => Ok(()),
        1 => Err(MessageError::Io("disk".into())),
        _ => Err(MessageError::Closed),
    }
}

#[uniffi::export(async_runtime = "tokio")]
pub async fn fetch_via(source: Box<dyn AsyncSource>, key: String) -> String {
    source.fetch(key).await
}

#[derive(Clone, uniffi::Record)]
pub struct Holder {
    pub counter: Arc<Counter>,
    pub others: Vec<Arc<Counter>>,
}

#[uniffi::export]
pub fn make_holder(start: i64) -> Holder {
    Holder { counter: Counter::new(start), others: vec![Counter::new(start + 1), Counter::new(start + 2)] }
}

#[uniffi::export]
pub fn echo_u64s(values: HashMap<String, Vec<u64>>) -> HashMap<String, Vec<u64>> {
    values
}
