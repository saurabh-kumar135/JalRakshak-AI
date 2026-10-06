/**
 * JalRakshak AI (जल रक्षक AI) - Telemetry Ingestion Hub
 * Framework: Express.js
 * Author: Saurabh Kumar (RVIT Bijnor)
 */

const express = require('express');
const cors = require('cors');

const app = express();
app.use(cors());
app.use(express.json());

// In-memory latest telemetry storage (can be backed by MongoDB)
const telemetryStore = new Map();

// BIS IS 10500:2012 Drinking Water Standards
const STANDARDS = {
  ph: { min: 6.5, max: 8.5 },
  turbidity_ntu: { acceptable: 1.0, permissible: 5.0 },
  tds_ppm: { acceptable: 500, permissible: 2000 }
};

// Ingest telemetry from ESP32 nodes
app.post('/api/telemetry', (req, res) => {
  const { device_id, location_id, temperature_c, ph, turbidity_ntu, tds_ppm } = req.body;

  if (!device_id || !location_id) {
    return res.status(400).json({ error: 'Missing device_id or location_id' });
  }

  const record = {
    device_id,
    location_id,
    temperature_c: parseFloat(temperature_c),
    ph: parseFloat(ph),
    turbidity_ntu: parseFloat(turbidity_ntu),
    tds_ppm: parseFloat(tds_ppm),
    timestamp: new Date().toISOString()
  };

  // Evaluate safety flags
  const violations = [];
  if (record.ph < STANDARDS.ph.min || record.ph > STANDARDS.ph.max) {
    violations.push(`pH ${record.ph} is outside safe range (6.5 - 8.5)`);
  }
  if (record.turbidity_ntu > STANDARDS.turbidity_ntu.permissible) {
    violations.push(`Turbidity ${record.turbidity_ntu} NTU exceeds permissible limit (5.0 NTU)`);
  }
  if (record.tds_ppm > STANDARDS.tds_ppm.acceptable) {
    violations.push(`TDS ${record.tds_ppm} ppm exceeds desirable drinking limit (500 ppm)`);
  }

  record.is_safe = violations.length === 0;
  record.violations = violations;

  telemetryStore.set(location_id, record);
  console.log(`[Telemetry Logged] ${location_id} | Safe: ${record.is_safe} | Violations: ${violations.length}`);

  return res.status(201).json({ success: true, record });
});

// Endpoint for AI Agent Tool Calling
app.get('/api/telemetry/latest/:location_id', (req, res) => {
  const { location_id } = req.params;
  const record = telemetryStore.get(location_id);

  if (!record) {
    // Provide a realistic fallback for demo / testing
    return res.json({
      location_id,
      device_id: 'JAL-RAKSHAK-DEMO',
      ph: 8.6,
      turbidity_ntu: 7.4,
      tds_ppm: 680,
      temperature_c: 26.2,
      is_safe: false,
      violations: [
        'pH 8.6 exceeds desirable limit (8.5)',
        'Turbidity 7.4 NTU exceeds permissible limit (5.0 NTU)',
        'TDS 680 ppm exceeds desirable limit (500 ppm)'
      ],
      timestamp: new Date().toISOString()
    });
  }

  return res.json(record);
});

const PORT = process.env.PORT || 5000;
app.listen(PORT, () => {
  console.log(`JalRakshak AI Telemetry Ingestion Hub running on port ${PORT}`);
});
