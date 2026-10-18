'use strict';
// При відкритті index.html як файлу зберігаємо демо; з ESP32 читаємо API.
const DEMO_MODE = location.protocol === 'file:';
const INTERVAL_MS = 10000;
const MAX_HISTORY = 10;
const $ = id => document.getElementById(id);
let readings = [];
let temperature = 22.5;
let humidity = 52.1;
let busy = false;
const nowLabel = () => new Date().toLocaleTimeString('uk-UA', {hour12: false});
const format = n => Number(n).toFixed(1);
const randomStep = range => (Math.random() * 2 - 1) * range;

function simulate() {
  temperature = Math.min(29, Math.max(17, temperature + randomStep(.35)));
  humidity = Math.min(75, Math.max(25, humidity + randomStep(.75)));
  readings.push({temperature, humidity, timestamp: Date.now()});
  readings = readings.slice(-MAX_HISTORY);
  render(true);
}

function drawChart(id, values, color) {
  const svg = $(id);
  if (!values.length) { svg.replaceChildren(); return; }
  const width = 800, height = 160, pad = 14;
  const min = Math.min(...values) - .8, max = Math.max(...values) + .8;
  const x = i => pad + i * (width - 2 * pad) / Math.max(1, MAX_HISTORY - 1);
  const y = v => height - pad - (v - min) / (max - min) * (height - 2 * pad);
  const points = values.map((v, i) => `${x(i)},${y(v)}`).join(' ');
  const area = `${x(0)},${height - pad} ${points} ${x(values.length - 1)},${height - pad}`;
  const grid = [.2,.5,.8].map(p => `<line x1="0" x2="800" y1="${p*height}" y2="${p*height}" stroke="#284136" stroke-dasharray="4 10"/>`).join('');
  const circles = values.map((v,i) => `<circle cx="${x(i)}" cy="${y(v)}" r="4" fill="${color}" stroke="#101815" stroke-width="2"/>`).join('');
  // SVG формується лише з перевірених чисел і фіксованих кольорів.
  svg.innerHTML = `${grid}<polygon points="${area}" fill="${color}" fill-opacity="0.08"/><polyline points="${points}" stroke="${color}" stroke-width="3" fill="none" stroke-linecap="round" stroke-linejoin="round"/>${circles}`;
}
function addLog(message) {
  const entry = document.createElement('div'); entry.className = 'log-entry';
  const time = document.createElement('span'); time.className = 'log-time'; time.textContent = `[${nowLabel()}]`;
  const text = document.createElement('span'); text.textContent = message;
  entry.append(time, text); $('log').prepend(entry);
  while ($('log').children.length > 4) $('log').lastElementChild.remove();
}
function render(sensorOk) {
  const latest = readings.at(-1);
  if (!latest) {
    $('temperature').textContent = '--.-'; $('humidity').textContent = '--.-';
    $('tempStatus').textContent = 'NO DATA'; $('humidityStatus').textContent = 'NO DATA';
    return;
  }
  $('temperature').textContent = format(latest.temperature);
  $('humidity').textContent = format(latest.humidity);
  $('tempStatus').textContent = sensorOk ? (latest.temperature < 18 || latest.temperature > 28 ? 'CHECK RANGE' : 'STABLE') : 'SENSOR ERROR';
  $('humidityStatus').textContent = sensorOk ? (latest.humidity < 30 || latest.humidity > 70 ? 'CHECK RANGE' : 'STABLE') : 'SENSOR ERROR';
  $('lastUpdate').textContent = nowLabel(); // час останньої успішної синхронізації UI
  $('chartTempLast').textContent = `${format(latest.temperature)} °C`;
  $('chartHumLast').textContent = `${format(latest.humidity)} %`;
  drawChart('tempChart', readings.map(x => x.temperature), '#8dfc70');
  drawChart('humidityChart', readings.map(x => x.humidity), '#57ddc6');
}
async function requestJson(url, options) {
  const response = await fetch(url, {cache:'no-store', ...options});
  if (!response.ok) throw new Error(`HTTP ${response.status}`);
  return response.json();
}
async function refresh(manual = false) {
  if (busy) return;
  if (DEMO_MODE) { simulate(); return; }
  busy = true;
  $('refreshButton').disabled = true;
  try {
    const current = await requestJson(manual ? '/api/refresh' : '/api/current', manual ? {method:'POST'} : undefined);
    const data = await requestJson('/api/history');
    if (!Array.isArray(data)) throw new Error('Invalid history');
    readings = data.filter(r => Number.isFinite(r.temperature) && Number.isFinite(r.humidity)).slice(-MAX_HISTORY);
    render(Boolean(current.sensorOk));
    $('connectionLabel').textContent = current.sensorOk ? 'SENSOR ONLINE' : 'SENSOR ERROR';
    if (current.sensorOk && readings.length) {
      const previous = refresh.lastTimestamp;
      const newest = current.timestamp;
      if (manual || newest !== previous) addLog(`Дані: ${format(current.temperature)} °C / ${format(current.humidity)} %`);
      refresh.lastTimestamp = newest;
    } else if (current.sensorOk === false && !refresh.errorNoted) {
      addLog('DHT22: немає актуального вимірювання');
    }
    refresh.errorNoted = !current.sensorOk;
  } catch (error) {
    $('connectionLabel').textContent = 'ESP32 OFFLINE';
    $('tempStatus').textContent = 'NO CONNECTION';
    $('humidityStatus').textContent = 'NO CONNECTION';
    if (!refresh.networkError) addLog(`Помилка API: ${error.message}`);
    refresh.networkError = true;
    return;
  } finally {
    busy = false;
    $('refreshButton').disabled = false;
  }
  refresh.networkError = false;
}
$('refreshButton').addEventListener('click', () => refresh(true));
$('intervalLabel').textContent = String(INTERVAL_MS / 1000);
$('clock').textContent = nowLabel();
setInterval(() => { $('clock').textContent = nowLabel(); }, 1000);
if (DEMO_MODE) {
  $('connectionLabel').textContent = 'DEMO MODE';
  for (let i = 0; i < MAX_HISTORY; i++) simulate();
  $('log').replaceChildren();
  addLog('Локальний демо-режим');
} else {
  addLog('Підключення до ESP32...');
  refresh();
}
setInterval(() => refresh(), INTERVAL_MS);
