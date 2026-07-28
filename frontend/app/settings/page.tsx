"use client";

import React, { useState } from "react";
import {
  Alert, Box, Button, Card, CardContent,
  Divider, Grid, Slider, Switch, TextField,
  Typography,
} from "@mui/material";
import SaveIcon from "@mui/icons-material/Save";

export default function SettingsPage() {
  const [host, setHost]         = useState("127.0.0.1");
  const [port, setPort]         = useState("9000");
  const [maxKeys, setMaxKeys]   = useState(100000);
  const [ttlEnabled, setTtlEnabled] = useState(true);
  const [saved, setSaved]       = useState(false);

  const handleSave = () => {
    setSaved(true);
    setTimeout(() => setSaved(false), 3000);
  };

  return (
    <Box>
      <Box sx={{ mb: 3 }}>
        <Typography variant="h5" sx={{ fontWeight: 700 }}>Settings</Typography>
        <Typography variant="body2" color="text.secondary">Configure cache server connection and behaviour</Typography>
      </Box>

      {saved && <Alert severity="success" sx={{ mb: 3 }}>Settings saved successfully.</Alert>}

      <Grid container spacing={3}>
        {/* Connection */}
        <Grid size={{ xs: 12, md: 6 }}>
          <Card sx={{ bgcolor: "background.paper", border: "1px solid rgba(255,255,255,0.07)", borderRadius: 3 }}>
            <CardContent>
              <Typography variant="subtitle2" sx={{ fontWeight: 600, mb: 2 }}>Connection</Typography>
              <TextField label="C++ Server Host" value={host} onChange={e => setHost(e.target.value)}
                fullWidth size="small" sx={{ mb: 2 }} />
              <TextField label="C++ Server Port" value={port} onChange={e => setPort(e.target.value)}
                fullWidth size="small" type="number" sx={{ mb: 2 }} />
              <TextField label="FastAPI Port" defaultValue="8000"
                fullWidth size="small" type="number" />
            </CardContent>
          </Card>
        </Grid>

        {/* Cache behaviour */}
        <Grid size={{ xs: 12, md: 6 }}>
          <Card sx={{ bgcolor: "background.paper", border: "1px solid rgba(255,255,255,0.07)", borderRadius: 3 }}>
            <CardContent>
              <Typography variant="subtitle2" sx={{ fontWeight: 600, mb: 2 }}>Cache Behaviour</Typography>

              <Typography gutterBottom>Max Keys: <strong>{maxKeys.toLocaleString()}</strong></Typography>
              <Slider value={maxKeys} min={1000} max={1000000} step={1000}
                onChange={(_, v) => setMaxKeys(v as number)} sx={{ mb: 3 }} />

              <Divider sx={{ my: 2, borderColor: "rgba(255,255,255,0.06)" }} />

              <Box sx={{ display: "flex", alignItems: "center", justifyContent: "space-between" }}>
                <Box>
                  <Typography variant="body2">TTL Expiry</Typography>
                  <Typography variant="caption" color="text.secondary">Auto-evict keys when TTL expires</Typography>
                </Box>
                <Switch checked={ttlEnabled} onChange={e => setTtlEnabled(e.target.checked)} />
              </Box>

              <Box sx={{ display: "flex", alignItems: "center", justifyContent: "space-between", mt: 2 }}>
                <Box>
                  <Typography variant="body2">LRU Eviction</Typography>
                  <Typography variant="caption" color="text.secondary">Remove least-recently-used keys when full</Typography>
                </Box>
                <Switch defaultChecked />
              </Box>
            </CardContent>
          </Card>
        </Grid>

        {/* Dashboard */}
        <Grid size={{ xs: 12, md: 6 }}>
          <Card sx={{ bgcolor: "background.paper", border: "1px solid rgba(255,255,255,0.07)", borderRadius: 3 }}>
            <CardContent>
              <Typography variant="subtitle2" sx={{ fontWeight: 600, mb: 2 }}>Dashboard</Typography>

              <Box sx={{ display: "flex", alignItems: "center", justifyContent: "space-between", mb: 2 }}>
                <Box>
                  <Typography variant="body2">Auto-refresh</Typography>
                  <Typography variant="caption" color="text.secondary">Update metrics automatically</Typography>
                </Box>
                <Switch defaultChecked />
              </Box>

              <TextField label="Refresh interval (ms)" defaultValue="2000"
                fullWidth size="small" type="number" />
            </CardContent>
          </Card>
        </Grid>

        {/* Save */}
        <Grid size={12}>
          <Button variant="contained" size="large" startIcon={<SaveIcon />} onClick={handleSave}>
            Save Settings
          </Button>
        </Grid>
      </Grid>
    </Box>
  );
}
