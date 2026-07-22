"use client";

import React, { useCallback, useEffect, useState } from "react";
import {
  Alert, Box, CircularProgress, IconButton,
  Snackbar, Tooltip, Typography,
} from "@mui/material";
import RefreshIcon from "@mui/icons-material/Refresh";
import KeyForm from "@/components/keys/KeyForm";
import KeyTable from "@/components/keys/KeyTable";
import api from "@/lib/apiClient";
import { KeyListItem, KeyListResponse } from "@/types/index";

export default function KeysPage() {
  const [keys, setKeys]       = useState<KeyListItem[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError]     = useState<string | null>(null);
  const [snack, setSnack]     = useState<string | null>(null);

  const fetchKeys = useCallback(async () => {
    setLoading(true);
    try {
      const res = await api.get<KeyListResponse>("/keys");
      setKeys(res.data.keys);
      setError(null);
    } catch {
      setError("Could not load keys — is the cache server running?");
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => { fetchKeys(); }, [fetchKeys]);

  const handleDelete = async (key: string) => {
    try {
      await api.delete(`/keys/${encodeURIComponent(key)}`);
      setSnack(`Deleted "${key}"`);
      fetchKeys();
    } catch {
      setSnack(`Failed to delete "${key}"`);
    }
  };

  return (
    <Box>
      <Box sx={{ display: "flex", alignItems: "center", justifyContent: "space-between", mb: 3 }}>
        <Box>
          <Typography variant="h5" sx={{ fontWeight: 700 }}>Key Explorer</Typography>
          <Typography variant="body2" color="text.secondary">
            Browse, set, and delete keys in the cache
          </Typography>
        </Box>
        <Tooltip title="Refresh keys">
          <IconButton onClick={fetchKeys}><RefreshIcon /></IconButton>
        </Tooltip>
      </Box>

      <KeyForm onKeySet={fetchKeys} />

      <Box sx={{ mt: 4 }}>
        <Typography variant="h6" sx={{ fontWeight: 600, mb: 2 }}>
          Stored Keys {!loading && `(${keys.length})`}
        </Typography>
        {error && <Alert severity="error" sx={{ mb: 2 }}>{error}</Alert>}
        {loading
          ? <Box sx={{ display: "flex", justifyContent: "center", py: 4 }}><CircularProgress /></Box>
          : <KeyTable keys={keys} onDelete={handleDelete} />
        }
      </Box>

      <Snackbar open={!!snack} autoHideDuration={3000}
        onClose={() => setSnack(null)} message={snack} />
    </Box>
  );
}
